#ifndef AFFIX_LOGIC_H
#define AFFIX_LOGIC_H
#include <stdint.h>
#include <math.h>
enum { AFF_STRONG=1, AFF_SPEEDY=2, AFF_REGEN=4, AFF_BERSERK=8, AFF_ENRAGED=16, AFF_HEAVYWEIGHT=32, AFF_VOLATILE=64, AFF_TRAIT_COUNT=7 };
static unsigned aff_roll(const double draws[AFF_TRAIT_COUNT],const double chances[AFF_TRAIT_COUNT]) { unsigned t=0;for(unsigned i=0;i<AFF_TRAIT_COUNT;i++)if(draws[i]<chances[i])t|=1u<<i;return t; }
static double aff_missing_health(int64_t hp,int64_t max){return max>0&&hp>0&&hp<=max?1.-(double)hp/max:0.;}
static double aff_motion_scale(unsigned traits,int64_t hp,int64_t max,double speedy,double enraged,double heavy){double result=(traits&AFF_SPEEDY)?speedy:1.;if(traits&AFF_ENRAGED)result*=1.+aff_missing_health(hp,max)*(enraged-1.);if(traits&AFF_HEAVYWEIGHT)result*=heavy;return fmin(10.,fmax(.1,result));}
static int64_t aff_initial_hp(int64_t hp,int64_t max,int64_t bonus){if(hp<=0||max<=0)return hp;if(hp>=max)return max;return bonus>=max-hp?max:hp+bonus;}
static double aff_hp_scale(unsigned traits,double ordinaryAffix,double strong,double heavy){double scale=0;if(traits&AFF_STRONG)scale=strong;if(traits&AFF_HEAVYWEIGHT)scale=fmax(scale,heavy);return scale>0?scale:(traits?ordinaryAffix:1.);}
static double aff_power_factor(unsigned traits,int64_t hp,int64_t max,double strong,double heavy,double berserk,double threshold){double factor=(traits&AFF_STRONG)?strong:(traits&AFF_HEAVYWEIGHT)?heavy:1.;if((traits&AFF_BERSERK)&&hp>0&&max>0&&(double)hp/max<threshold)factor*=berserk;return factor;}
static uint32_t aff_blast_damage(uint32_t nativeDamage,double base,double user){return (uint32_t)fmin(2147483647.,floor(nativeDamage*base*user));}
static double aff_reward_total(unsigned traits,const double rewards[AFF_TRAIT_COUNT]){double total=0;for(unsigned i=0;i<AFF_TRAIT_COUNT;i++)if(traits&(1u<<i))total+=rewards[i];return total;}
static int aff_reward_roll(double percent,double draw){return percent>=100||(percent>0&&draw<percent/100.);}
static int32_t aff_reward_cash(int32_t tableAmount,double baseDollars){double total=floor((double)tableAmount*baseDollars/500.+.5);return (int32_t)fmin(INT32_MAX,fmax(0,total));}
static unsigned aff_count(unsigned t) {return !!(t&1)+!!(t&2)+!!(t&4)+!!(t&8)+!!(t&16)+!!(t&32)+!!(t&64);}
static int aff_elite(unsigned t) {return aff_count(t)>=2;}
static int64_t aff_heal(int64_t hp,int64_t max,double dt,double rate,double *credit) {
 if(hp<=0||max<=0||hp>=max){*credit=0;return hp;}
 if(!isfinite(dt)||dt<=0||dt>.25||!isfinite(rate)||rate<0)return hp;
 *credit+=(double)max*rate*dt;int64_t whole=(int64_t)floor(*credit);*credit-=whole;
 if(whole>=max-hp){*credit=0;return max;}return hp+whole;
}
#endif
