#include <assert.h>
#include <stdio.h>
#include "affix_logic.h"
int main(void){
 for(unsigned n=0;n<=7;n++){assert(aff_reward_cash(50000,500*n)==(int32_t)(50000*n));assert(aff_reward_cash(500000,500*n)==(int32_t)(500000*n));}
 assert(aff_reward_cash(INT32_MAX,1500)==INT32_MAX);
 double rewards[AFF_TRAIT_COUNT]={100,200,300,400,500,600,700};
 assert(!aff_reward_roll(0,0));assert(aff_reward_roll(100,.999999));assert(aff_reward_roll(50,.499));assert(!aff_reward_roll(50,.5));
 assert(aff_reward_cash(500000,750)==750000);assert(aff_reward_cash(50000,750)==75000);assert(aff_reward_cash(500000,0)==0);assert(aff_reward_cash(INT32_MAX,700000)==INT32_MAX);
 double chances[AFF_TRAIT_COUNT]={.2,.2,.2,.1,.1,.1,.08};
 for(unsigned mask=0;mask<128;mask++){
  double draws[AFF_TRAIT_COUNT];unsigned count=0;
  for(unsigned i=0;i<AFF_TRAIT_COUNT;i++){draws[i]=(mask&(1u<<i))?.01:.9;count+=(mask>>i)&1;}
  assert(aff_roll(draws,chances)==mask);assert(aff_count(mask)==count);assert(aff_elite(mask)==(count>=2));
  assert(aff_hp_scale(mask,2.5,6,6)==((mask&(AFF_STRONG|AFF_HEAVYWEIGHT))?6:mask?2.5:1));
  double expected=0;for(unsigned i=0;i<AFF_TRAIT_COUNT;i++)if(mask&(1u<<i))expected+=rewards[i];assert(aff_reward_total(mask,rewards)==expected);
  assert(aff_reward_cash(500000,500*count)==(int32_t)(500000*count));
 }
 assert(aff_motion_scale(AFF_ENRAGED,100,100,3.5,2,.75)==1);
 assert(aff_motion_scale(AFF_ENRAGED,50,100,3.5,2,.75)==1.5);
 assert(aff_motion_scale(AFF_HEAVYWEIGHT,100,100,3.5,2,.75)==.75);
 assert(aff_motion_scale(AFF_HEAVYWEIGHT|AFF_ENRAGED|AFF_SPEEDY,50,100,3.5,2,.75)==3.9375);
 assert(aff_motion_scale(AFF_SPEEDY|AFF_ENRAGED,1,100,10,5,.75)==10);
 assert(aff_power_factor(AFF_BERSERK,50,100,3,1.5,2,.5)==1);
 assert(aff_power_factor(AFF_BERSERK,49,100,3,1.5,2,.5)==2);
 assert(aff_power_factor(AFF_STRONG|AFF_BERSERK,49,100,3,1.5,2,.5)==6);
 assert(aff_power_factor(AFF_STRONG|AFF_BERSERK,51,100,3,1.5,2,.5)==3);
 assert(aff_power_factor(AFF_BERSERK,0,100,3,1.5,2,.5)==1);
 assert(aff_power_factor(AFF_BERSERK,10,0,3,1.5,2,.5)==1);
 assert(aff_initial_hp(2560,25600,23040)==25600);
 assert(aff_initial_hp(2500,25600,23040)==25540); /* preserve concurrent damage */
 assert(aff_initial_hp(0,25600,23040)==0);
 assert(aff_initial_hp(5000,6400,6000)==6400);
 assert(aff_hp_scale(AFF_STRONG|AFF_SPEEDY,7.5,12.5,6)==12.5);assert(aff_hp_scale(AFF_REGEN,7.5,12.5,6)==7.5);assert(aff_hp_scale(0,7.5,12.5,6)==1.);
 assert(aff_power_factor(AFF_HEAVYWEIGHT,100,100,3,1.5,2,.5)==1.5);
 assert(aff_power_factor(AFF_HEAVYWEIGHT|AFF_STRONG,100,100,3,1.5,2,.5)==3);
 assert(aff_power_factor(AFF_HEAVYWEIGHT|AFF_BERSERK,40,100,3,1.5,2,.5)==3);
 assert(aff_hp_scale(AFF_HEAVYWEIGHT|AFF_STRONG,2.5,6,8)==8);
 assert(aff_motion_scale(AFF_HEAVYWEIGHT,100,100,3.5,4,.5)==.5);
 assert(aff_motion_scale(AFF_ENRAGED,50,100,3.5,4,.5)==2.5);
 assert(aff_blast_damage(300,5,5)==7500);
 assert(aff_blast_damage(300,5,5)==5*aff_blast_damage(300,1,5));
 assert(aff_blast_damage(UINT32_MAX,20,5)==INT32_MAX);
 double halfCredit=0;assert(aff_heal(1000,6400,.25,.05,&halfCredit)==1080);
 double credit=0;int64_t hp=50;for(int i=0;i<60;i++)hp=aff_heal(hp,100,1./60,.01,&credit);assert(hp==51);
 assert(aff_heal(0,100,.25,1,&credit)==0);assert(aff_heal(99,100,.25,1,&credit)==100);assert(aff_heal(50,100,0,1,&credit)==50);assert(aff_heal(50,100,NAN,1,&credit)==50);
 puts("Native core: all 128 trait combinations, Enraged/Heavyweight playback, Berserk threshold/stacking/recovery, money tiers, fractional healing, cap, pause and death checks passed.");return 0;
}
