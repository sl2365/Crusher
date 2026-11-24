#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// Original vintage ADC/DAC model. Profiles vary input headroom, pre-emphasis,
// aperture, converter ladder mismatch and reconstruction, not just presets
// of the existing Lo-Fi engine. No random noise or audio-thread allocations.
namespace crusher::sampler
{
inline constexpr std::array<const char*, 6> modelNames {
    "Warm", "Punch", "Dark", "Groove", "Pocket", "Arcade" };
inline double maximumRate(double sr) noexcept { return std::clamp(sr, 4000.0, 48000.0); }
inline double rateHz(double p, double sr) noexcept
{
    const double hi = maximumRate(sr);
    return hi * std::pow(4000.0 / hi, std::clamp(p, 0.0, 1.0));
}
struct Controls { float model = 0.0f, rate = 0.35f, bits = 0.67f, colour = 0.5f, punch = 0.5f; };
class Processor
{
    // Input gain, pre-emphasis, capture aperture, ladder error, DAC slew,
    // reconstruction damping, dynamic compression. Interpolated continuously.
    using Profile = std::array<float, 7>;
    static constexpr std::array<Profile, 6> profiles {{
        {{1.35f,  0.12f, 0.70f, 0.10f, 0.65f, 0.75f, 0.55f}},
        {{2.10f,  0.65f, 0.35f, 0.32f, 0.85f, 0.35f, 0.90f}},
        {{1.55f, -0.35f, 0.96f, 0.14f, 0.35f, 0.95f, 0.40f}},
        {{1.85f,  0.30f, 0.82f, 0.40f, 0.55f, 0.68f, 1.00f}},
        {{2.40f,  0.95f, 0.18f, 0.75f, 0.90f, 0.25f, 0.30f}},
        {{3.20f,  0.50f, 0.02f, 0.95f, 1.00f, 0.02f, 0.10f}}
    }};
    struct State { float phase=0.0f, pre=0, last=0, previousInput=0, held=0, dac=0, low=0, low2=0, dcIn=0, dcOut=0, fast=0, slow=0; };
public:
    void prepare(double sr) noexcept
    {
        sampleRate = static_cast<float>(std::max(4000.0, sr));
        constexpr float tau=6.28318530718f;
        for (std::size_t i=0; i<rates.size(); ++i)
            rates[i]=static_cast<float>(rateHz(double(i)/(rates.size()-1),sampleRate)/sampleRate);
        preCoeff=1.0f-std::exp(-tau*std::min(1100.0f,sampleRate*0.3f)/sampleRate);
        fastCoeff=1.0f-std::exp(-1.0f/(0.0015f*sampleRate));
        slowCoeff=1.0f-std::exp(-1.0f/(0.045f*sampleRate));
        dcCoeff=std::exp(-tau*9.0f/sampleRate);
        previous={-1,-1,-1,-1,-1}; setControls({}); reset();
    }
    void reset() noexcept { states={}; }
    void setControls(const Controls& c) noexcept
    {
        if (c.model==previous.model && c.rate==previous.rate && c.bits==previous.bits && c.colour==previous.colour && c.punch==previous.punch) return;
        const float m=5.0f*unit(c.model); const auto a=static_cast<std::size_t>(m); const auto b=std::min(a+1,std::size_t(5));
        for (std::size_t i=0;i<profile.size();++i) profile[i]=profiles[a][i]+(profiles[b][i]-profiles[a][i])*(m-float(a));
        const float r=unit(c.rate)*float(rates.size()-1); const auto ri=static_cast<std::size_t>(r);
        increment=rates[ri]+(rates[std::min(ri+1,rates.size()-1)]-rates[ri])*(r-float(ri));
        const float bp=4.0f+12.0f*unit(c.bits); bits=std::clamp(static_cast<int>(bp),4,16); bitMix=bp-float(bits);
        levels=float((1u<<(bits-1))-1u); nextLevels=float((1u<<(std::min(bits+1,16)-1))-1u);
        colour=unit(c.colour); punch=unit(c.punch);
        // Stable one-pole reconstruction coefficients track converter clock.
        apertureCoeff=std::clamp(increment*(1.4f-0.8f*profile[2]),0.001f,1.0f);
        reconCoeff=std::clamp(increment*(0.65f+1.8f*profile[4])*(1.35f-0.65f*colour),0.001f,1.0f);
        previous=c;
    }
    float process(float input,float drive,int channel) noexcept
    {
        const float wet=unit(drive); if(wet==0) return input;
        auto& s=states[static_cast<std::size_t>(std::clamp(channel,0,1))];
        const float x=std::isfinite(input)?std::clamp(input,-8.0f,8.0f):0.0f;
        const float low=advance(s.pre,preCoeff,x);
        const float env=std::abs(x); advance(s.fast,fastCoeff,env); advance(s.slow,slowCoeff,env);
        const float transient=unit((s.fast-s.slow)/(0.035f+s.slow)*1.7f);
        // Analogue input headroom and dynamic loading; colour governs depth.
        const float gain=1.0f+colour*(profile[0]-1.0f)*1.7f;
        const float emphasized=x+(x-low)*profile[1]*colour*1.65f;
        const float loaded=emphasized*gain/(1.0f+colour*profile[6]*s.slow*2.0f);
        const float bounded=std::clamp(loaded/(1.0f+0.55f*colour*std::abs(loaded)), -1.0f,1.0f);
        // Aperture averaging and fractional clock interpolation capture the
        // crossing rather than the nearest host sample.
        const float filtered=advance(s.last,apertureCoeff,bounded);
        s.phase+=increment;
        if(s.phase>=1.0f)
        {
            s.phase-=1.0f; const float frac=unit(s.phase/std::max(increment,0.0001f));
            const float crossing=bounded+(s.previousInput-bounded)*frac;
            const float captured=crossing+(filtered-crossing)*profile[2];
            const float q=convert(captured,levels,colour*profile[3]);
            const float q2=convert(captured,nextLevels,colour*profile[3]);
            s.held=q+(q2-q)*bitMix;
        }
        s.previousInput=bounded;
        // DAC settling and multi-pole reconstruction vary by converter family.
        const float dac=advance(s.dac,reconCoeff,s.held);
        const float first=advance(s.low,reconCoeff,dac);
        const float second=advance(s.low2,reconCoeff,first);
        const float reconstructed=dac+(second-dac)*profile[5];
        float y=s.held+(reconstructed-s.held)*(0.12f+0.86f*profile[5]);
        // Deterministic analog output coloration and DC removal.
        y*=1.0f+0.6f*colour;
        const float hp=y-s.dcIn+dcCoeff*s.dcOut; s.dcIn=y; s.dcOut=std::abs(hp)<1.0e-16f?0.0f:hp;
        y=s.dcOut+(x-s.dcOut)*punch*(0.12f+0.82f*transient);
        const float out=x+(y-x)*wet;
        return std::isfinite(out)?std::clamp(out,-16.0f,16.0f):0.0f;
    }
private:
    static float unit(float v) noexcept { return std::isfinite(v)?std::clamp(v,0.0f,1.0f):0.0f; }
    static float advance(float& state,float c,float x) noexcept { state+=c*(x-state); if(std::abs(state)<1.0e-16f) state=0; return state; }
    static float convert(float x,float n,float mismatch) noexcept
    {
        // Non-uniform DAC ladder: deterministic code-dependent errors, zero
        // code always maps to zero. No dither/static and no random modulation.
        const int code=static_cast<int>(std::round(std::clamp(x,-1.0f,1.0f)*n));
        if(code==0) return 0;
        const int mag=std::abs(code);
        const float error=mismatch*(0.6f*float((mag%7)-3)+0.3f*float((mag%3)-1));
        const float v=(float(mag)+error)/n;
        return std::clamp(code<0?-v:v,-1.0f,1.0f);
    }
    std::array<State,2> states{}; std::array<float,1025> rates{}; Profile profile{};
    Controls previous{-1,-1,-1,-1,-1}; float sampleRate=44100,increment=1,preCoeff=0.1f,fastCoeff=0.01f,slowCoeff=0.001f,dcCoeff=0.999f;
    float apertureCoeff=1,reconCoeff=1,colour=0.5f,punch=0.5f,levels=511,nextLevels=1023,bitMix=0; int bits=10;
};
} // namespace crusher::sampler
