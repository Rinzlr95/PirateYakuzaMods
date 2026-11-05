#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "minhook/include/MinHook.h"
#include "affix_logic.h"
// Steam 1.14 only. Addresses are RVAs, validated against the supplied executable.
#define REGISTRY 0x3885E40ull
#define INFO_VT 0x2B0F100ull
#define FIGHTER_VT 0x2AD1B88ull
#define GAUGE_VT 0x2BED228ull
#define BRAIN_VT 0x2AD5B58ull
#define HEALTH_VT 0x2AD1B68ull
#define MOTION_VT 0x2BA8770ull
#define BONUS_REWARD_ROW 244u
static uintptr_t image;
static HMODULE selfModule;
static CRITICAL_SECTION stateLock;
static DWORD tlsIndex=TLS_OUT_OF_INDEXES;
static double affixHp=2.5,strongHp=6.,strongPower=3.,speedScale=3.5;
static double heavyHp=6.,heavyPower=1.5,volatileBaseDamage=5.,enragedMaxSpeed=4.,enragedIncoming=1.5,heavySpeed=.5,volatileDelay=1.5,volatileRadius=5.,volatileDamage=5.;
static double berserkPower=2.,berserkThreshold=.5;
static int showHealthNumbers=1;
static double rewardValues[AFF_TRAIT_COUNT]={500,500,500,500,500,500,500},rewardDropChance=100.;
static const char *rewardKeys[AFF_TRAIT_COUNT]={"StrongReward","SpeedyReward","RegenReward","BerserkReward","EnragedReward","HeavyweightReward","VolatileReward"};
static double chances[AFF_TRAIT_COUNT]={.2,.2,.2,.10,.10,.10,.08},regenRate=.03;
static FILE *logFile;
static unsigned long spawnCount;
static LARGE_INTEGER qpcFrequency;
static volatile LONG ready;
static DWORD gameplayThread;
static volatile LONG tickCalls,defeatCalls,rowCalls,cashCalls;
static volatile LONG rejected[8];
static char iniPath[MAX_PATH],logPath[MAX_PATH];
typedef struct State State;
struct State {State *next;uint32_t owner,infoId,fighterId;uintptr_t info,fighter,brain;unsigned traits;float baseDefence,writtenDefence;int armourLogged,explosionPending;double explosionWait;float explosionPosition[4];float basePower,writtenPower;int berserkActive;int hpScaled,hpUiLogged;int64_t pendingHpBonus;int applied,paid,refs,speedLogged,regenLogged,boss;uint32_t aiId;float motion,shift,wait;float writtenMotion,writtenShift,writtenWait;double credit;LONGLONG lastRegen,lastLabel;uint64_t uiString,labelHandle;uint32_t labelGauge;int regenHealedLogged,speedReadbackLogged;char *label;};
static State *states;
typedef void (__fastcall *TickFn)(void*);
typedef void* (__fastcall *NameFn)(void*,void*);
typedef void (__fastcall *DefeatFn)(void*,const void*);
typedef void (__fastcall *RowFn)(void*,const void*,uint32_t);
typedef void* (__fastcall *GetInfoFn)(void*,uint32_t*);
static TickFn originalTick;
static NameFn originalName;
static DefeatFn originalDefeat;
static RowFn originalRow;
static GetInfoFn getInfo;
typedef void (__fastcall *StringAssignFn)(uint64_t*,const char*);
typedef void (__fastcall *GaugeTextFn)(void*,const uint64_t*);
typedef void (__fastcall *CashFn)(void*,int32_t,const void*,const void*);
static StringAssignFn stringAssign;
static GaugeTextFn setGaugeText;
static CashFn originalCash;
typedef void (__fastcall *MotionSpeedFn)(void*,float);
static MotionSpeedFn originalMotionSpeed;
typedef void* (__fastcall *CreateBombFn)(uintptr_t*,const float*,const void*);
static CreateBombFn createBomb;
typedef struct Scope {struct Scope *previous;State *state;int item,money,phase;int64_t bonusUnits;double baseReward;} Scope;
static int readmem(uintptr_t p,void *out,SIZE_T n) {SIZE_T got=0;return p>=65536&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static uint32_t u32(uintptr_t p){uint32_t v=0;readmem(p,&v,4);return v;}
static uintptr_t ptr(uintptr_t p){uintptr_t v=0;readmem(p,&v,8);return v;}
static int64_t i64(uintptr_t p){int64_t v=0;readmem(p,&v,8);return v;}
static float f32(uintptr_t p){float v=NAN;readmem(p,&v,4);return v;}
static void note(const char *s){if(logFile){fprintf(logFile,"%s\n",s);fflush(logFile);}}
static uintptr_t resolve(uint32_t uid,uint16_t kind){uint32_t index=uid&0xfffff;unsigned char row[32];if(!uid||index>=0x80000||!readmem(image+REGISTRY+(uintptr_t)index*32,row,32))return 0;uint16_t gen,k;memcpy(&gen,row+8,2);memcpy(&k,row+12,2);uintptr_t address;memcpy(&address,row,8);if(gen!=(uid>>20)||k!=kind||u32(address+12)!=uid)return 0;return address;}
static int component(uintptr_t p,uintptr_t vt,uint32_t owner,uint16_t type){unsigned char b[48];uint64_t v;uint32_t o;uint16_t t;if(!readmem(p,b,48))return 0;memcpy(&v,b,8);memcpy(&t,b+36,2);memcpy(&o,b+40,4);return v==image+vt&&t==type&&o==owner&&resolve(u32(p+12),4)==p;}
static uintptr_t infoFor(uintptr_t actor,uint32_t owner){uint32_t uid=0;getInfo((void*)(actor+0x68),&uid);uintptr_t info=resolve(uid,4);return component(info,INFO_VT,owner,68)?info:0;}
static uintptr_t gaugeFor(uintptr_t actor,uint32_t owner){uintptr_t info=infoFor(actor,owner);if(!info)return 0;uintptr_t g=resolve(u32(info+0xC8),4);return component(g,GAUGE_VT,owner,74)?g:0;}
static void rejection(unsigned reason,uint32_t owner,uintptr_t info,uintptr_t fighter,uintptr_t gauge){if(reason>=8||InterlockedIncrement(&rejected[reason])!=1)return;EnterCriticalSection(&stateLock);if(logFile){fprintf(logFile,"filter reason=%u owner=%08X info=%p fighter=%p gauge=%p\n",reason,owner,(void*)info,(void*)fighter,(void*)gauge);fflush(logFile);}LeaveCriticalSection(&stateLock);}
static State *findState(uint32_t owner,uint32_t infoId,uint32_t fighterId){for(State*s=states;s;s=s->next)if(s->owner==owner&&s->infoId==infoId&&s->fighterId==fighterId)return s;return NULL;}
// Caller holds stateLock. No template/name/location whitelist is used.
static State *ensure(uint32_t owner,uintptr_t info,uintptr_t gauge,int allowDead){
 uintptr_t actor=resolve(owner,3);if(!actor||!component(info,INFO_VT,owner,68)){rejection(0,owner,info,0,gauge);return NULL;}
 uintptr_t fighter=resolve(u32(actor+0x4EB8),4);if(!component(fighter,FIGHTER_VT,owner,70)||ptr(fighter+0xD10)!=image+HEALTH_VT){rejection(1,owner,info,fighter,gauge);return NULL;}
 uint32_t control=u32(fighter+0xE0),category=u32(fighter+0xCFC);unsigned char force=255;readmem(info+0xD0,&force,1);
 if(control!=3||(category!=2&&category!=3)||force!=0){rejection(2,owner,info,fighter,gauge);return NULL;}
 uint32_t infoId=u32(info+12),fighterId=u32(fighter+12);State*s=findState(owner,infoId,fighterId);if(s){if(!allowDead&&i64(fighter+0xD20)<=0)return NULL;return s;}
 if(!gauge){rejection(3,owner,info,fighter,gauge);return NULL;}uint32_t skin=u32(gauge+0x74);if(skin!=1&&skin!=3&&skin!=9){rejection(4,owner,info,fighter,gauge);return NULL;}
 int64_t hp=i64(fighter+0xD20),max=i64(fighter+0xD18);if(max<=0||hp>max||(!allowDead&&hp<=0)){rejection(5,owner,info,fighter,gauge);return NULL;}
 if(hp<=0)return NULL;
 if((spawnCount&31)==0){State **link=&states;while(*link){State *old=*link;if(!old->refs&&(resolve(old->owner,3)==0||resolve(old->infoId,4)!=old->info||resolve(old->fighterId,4)!=old->fighter)){*link=old->next;/* Immutable name buffers remain alive until process exit. */free(old);}else link=&old->next;}}
 s=calloc(1,sizeof(*s));if(!s)return NULL;double draws[AFF_TRAIT_COUNT];uint32_t random[AFF_TRAIT_COUNT];if(BCryptGenRandom(NULL,(PUCHAR)random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0){free(s);return NULL;}
 for(int i=0;i<AFF_TRAIT_COUNT;i++)draws[i]=random[i]/4294967296.0;s->traits=aff_roll(draws,chances);s->boss=category==3||skin==3||skin==9;s->owner=owner;s->infoId=infoId;s->fighterId=fighterId;s->info=info;s->fighter=fighter;s->aiId=UINT32_MAX;s->next=states;states=s;
 if(logFile){fprintf(logFile,"spawn %lu owner=%08X template=%u traits=%u elite=%d hp=%lld/%lld\n",++spawnCount,owner,u32(info+0x74),s->traits,aff_elite(s->traits),(long long)hp,(long long)max);fflush(logFile);}return s;
}
static int stillValid(State*s){return resolve(s->owner,3)&&resolve(s->infoId,4)==s->info&&resolve(s->fighterId,4)==s->fighter&&ptr(s->fighter+0xD10)==image+HEALTH_VT;}
static void applyFloat(uintptr_t address,float baseline,float scale,float *last){float current=f32(address);if(!isfinite(current)||current<=0||current>1000)return;float v=baseline*scale;if(*last>0&&current==*last&&current==v)return;SIZE_T done;if(isfinite(v)&&v>0&&v<=1000&&WriteProcessMemory(GetCurrentProcess(),(void*)address,&v,4,&done)&&done==4)*last=v;}
static void apply(State*s,int gameplayTick){
 if(!stillValid(s))return;uintptr_t fighter=s->fighter;
 if(!s->applied){
  if(s->traits&(AFF_STRONG|AFF_BERSERK|AFF_HEAVYWEIGHT)){float power=f32(s->info+0xB0);if(isfinite(power)&&power>0&&power<100)s->basePower=power;}
  s->applied=1;
 }
 if(!s->hpScaled){
  double scale=aff_hp_scale(s->traits,affixHp,strongHp,heavyHp);
  int64_t oldMax=i64(fighter+0xD18),oldHp=i64(fighter+0xD20);
  if(scale==1)s->hpScaled=1;
  else if(oldMax>0&&oldMax<1000000000&&oldHp>0&&oldHp<=oldMax){
   int64_t newMax=(int64_t)floor(oldMax*scale);
   if(InterlockedCompareExchange64((volatile LONG64*)(fighter+0xD18),newMax,oldMax)==oldMax){
    s->pendingHpBonus=(int64_t)floor(oldHp*(scale-1));s->hpScaled=1;
    if(logFile){fprintf(logFile,"hp_scale_begin owner=%08X scale=%.3f hp_before=%lld max_before=%lld bonus=%lld\n",s->owner,scale,(long long)oldHp,(long long)oldMax,(long long)s->pendingHpBonus);fflush(logFile);}
   }
  }
 }
 // Apply the initial HP bonus once. Retry concurrent changes, preserving damage
 // already taken. Never resurrect, repeatedly refill, or compound the bonus.
 if(s->pendingHpBonus>0){
  for(unsigned attempt=0;attempt<16;attempt++){
   int64_t hp=i64(fighter+0xD20),max=i64(fighter+0xD18);
   if(hp<=0){s->pendingHpBonus=0;break;}
   int64_t target=aff_initial_hp(hp,max,s->pendingHpBonus);
   if(InterlockedCompareExchange64((volatile LONG64*)(fighter+0xD20),target,hp)==hp){
    s->pendingHpBonus=0;
    if(logFile){fprintf(logFile,"hp_applied owner=%08X hp_after=%lld max_hp_after=%lld\n",s->owner,(long long)i64(fighter+0xD20),(long long)i64(fighter+0xD18));fflush(logFile);}break;
   }
  }
 }
 if(s->basePower>0){
  int64_t hp=i64(fighter+0xD20),max=i64(fighter+0xD18);
  double factor=aff_power_factor(s->traits,hp,max,strongPower,heavyPower,berserkPower,berserkThreshold);
  float written=s->writtenPower;applyFloat(s->info+0xB0,s->basePower,(float)factor,&written);s->writtenPower=written;
  int active=(s->traits&AFF_BERSERK)&&hp>0&&max>0&&(double)hp/max<berserkThreshold;
  if(active!=s->berserkActive&&logFile){fprintf(logFile,"berserk owner=%08X active=%d hp=%lld max=%lld power_after=%.5f\n",s->owner,active,(long long)hp,(long long)max,f32(s->info+0xB0));fflush(logFile);}s->berserkActive=active;
 }
 if(gameplayTick&&(s->traits&AFF_REGEN)){float dt=f32(image+0x4885E44);LARGE_INTEGER now;QueryPerformanceCounter(&now);double elapsed=s->lastRegen?(double)(now.QuadPart-s->lastRegen)/qpcFrequency.QuadPart:0;
  if(!s->regenLogged&&logFile){fprintf(logFile,"regen owner=%08X first_game_delta=%.8f rate=%.5f\n",s->owner,dt,regenRate);fflush(logFile);s->regenLogged=1;}if(!s->lastRegen){s->lastRegen=now.QuadPart;return;}if(!isfinite(dt)||dt<=0||dt>.25){s->lastRegen=now.QuadPart;return;}if(elapsed<dt*.5)return;s->lastRegen=now.QuadPart;
  int64_t hp=i64(fighter+0xD20),max=i64(fighter+0xD18);double credit=s->credit;int64_t healed=aff_heal(hp,max,dt,regenRate,&credit);if(healed==hp||InterlockedCompareExchange64((volatile LONG64*)(fighter+0xD20),healed,hp)==hp){s->credit=credit;if(healed>hp&&!s->regenHealedLogged&&logFile){fprintf(logFile,"regen_applied owner=%08X hp_before=%lld hp_after=%lld\n",s->owner,(long long)hp,(long long)healed);fflush(logFile);s->regenHealedLogged=1;}}
 }
}
static float motionScale(State*s){return (float)aff_motion_scale(s->traits,i64(s->fighter+0xD20),i64(s->fighter+0xD18),speedScale,enragedMaxSpeed,heavySpeed);}
static void applyNewTraits(State*s){
 if(!stillValid(s)||i64(s->fighter+0xD20)<=0)return;
 if(s->traits&AFF_HEAVYWEIGHT){
  InterlockedOr64((volatile LONG64*)(s->fighter+0x940),3); // super + hard armor, NOT invulnerability (0x10)
  if(!s->armourLogged&&logFile){fprintf(logFile,"heavyweight_applied owner=%08X flags=%llX\n",s->owner,(unsigned long long)ptr(s->fighter+0x940));fflush(logFile);s->armourLogged=1;}
 }
 if(s->traits&AFF_ENRAGED){float current=f32(s->fighter+0x910);
  if(isfinite(current)&&current>0&&current<100){
   if(s->baseDefence<=0||current!=s->writtenDefence)s->baseDefence=current;
   double missing=aff_missing_health(i64(s->fighter+0xD20),i64(s->fighter+0xD18));
   applyFloat(s->fighter+0x910,s->baseDefence,(float)(1.+missing*(enragedIncoming-1.)),&s->writtenDefence);
  }
 }
}
// The stock 5DFD50 wrapper constructs native transforms/owner handles and
// invokes the same bomb factory used by game attacks. Retain no bomb pointer:
// its manager can move the backing array on the next creation.
static void processExplosions(void){
 static LONGLONG last;float dt=f32(image+0x4885E44);LARGE_INTEGER now;QueryPerformanceCounter(&now);
 if(!last||!isfinite(dt)||dt<=0||dt>.25){last=now.QuadPart;return;}
 if((double)(now.QuadPart-last)/qpcFrequency.QuadPart<dt*.5)return;last=now.QuadPart;
 for(State*s=states;s;s=s->next){if(!s->explosionPending)continue;s->explosionWait-=dt;if(s->explosionWait>0)continue;
  s->explosionPending=0;uintptr_t actor=resolve(s->owner,3);
  if(actor){unsigned char spec[32]={0};uint32_t row=898,power=100;memcpy(spec,&row,4);memcpy(spec+0x14,&power,4);
   uintptr_t wrapper=actor;uintptr_t bomb=(uintptr_t)createBomb(&wrapper,s->explosionPosition,spec);
   // Native decoded bomb fields: current/init/end radii at EC/F0/F4.
   // 62CB3E/77/7F write these from DB radius columns; 633530 copies them.
   if(bomb&&ptr(bomb+0xA0)==image+0x2ACDD08){SIZE_T done;float radius=(float)volatileRadius;
    uint32_t oldDamage=u32(bomb+0x10C);uint32_t damage=aff_blast_damage(oldDamage,volatileBaseDamage,volatileDamage);
    WriteProcessMemory(GetCurrentProcess(),(void*)(bomb+0x10C),&damage,4,&done);
    WriteProcessMemory(GetCurrentProcess(),(void*)(bomb+0xEC),&radius,4,&done);
    WriteProcessMemory(GetCurrentProcess(),(void*)(bomb+0xF0),&radius,4,&done);
    WriteProcessMemory(GetCurrentProcess(),(void*)(bomb+0xF4),&radius,4,&done);
   }
   if(logFile){fprintf(logFile,"volatile_explosion owner=%08X bomb=%p radius=%.3f row=898\n",s->owner,(void*)bomb,volatileRadius);fflush(logFile);}
  }else if(logFile){fprintf(logFile,"volatile_skipped owner=%08X reason=actor_despawned\n",s->owner);fflush(logFile);}
  s->refs--;
 }
}
// Actor +9F8 is ECMotion (828310); stock callers pass it to 163B590.
// Match a live rolled enemy before overriding any stock playback-rate reset.
static int speedyMotion(State*s,uintptr_t motion){uintptr_t actor=resolve(s->owner,3);return (s->traits&(AFF_SPEEDY|AFF_ENRAGED|AFF_HEAVYWEIGHT))&&actor&&stillValid(s)&&i64(s->fighter+0xD20)>0&&ptr(actor+0x9F8)==motion&&ptr(motion)==image+MOTION_VT;}
static void __fastcall hookMotionSpeed(void *motion,float speed){
 if(ready&&isfinite(speed)&&speed>0){EnterCriticalSection(&stateLock);for(State*s=states;s;s=s->next)if(speedyMotion(s,(uintptr_t)motion)){speed=motionScale(s);break;}LeaveCriticalSection(&stateLock);}
 originalMotionSpeed(motion,speed);
 if(ready&&gameplayThread==GetCurrentThreadId()){EnterCriticalSection(&stateLock);processExplosions();LeaveCriticalSection(&stateLock);}
}
static void applyMotion(State*s){
 EnterCriticalSection(&stateLock);uintptr_t actor=resolve(s->owner,3),motion=actor?ptr(actor+0x9F8):0;
 if(speedyMotion(s,motion)){unsigned char mode=0;float current=NAN;readmem(motion+0x142,&mode,1);current=f32(motion+(mode?0x130:0x12C));
  if(isfinite(current)&&current>0){originalMotionSpeed((void*)motion,motionScale(s));
   if(!s->speedReadbackLogged&&logFile){fprintf(logFile,"animation_speed_applied owner=%08X controller=%p before=%.5f after=%.5f\n",s->owner,(void*)motion,current,f32(motion+(mode?0x130:0x12C)));fflush(logFile);s->speedReadbackLogged=1;}
  }
 }LeaveCriticalSection(&stateLock);
}
static void refreshGaugeName(State*s,uintptr_t gauge);
static void __fastcall hookTick(void *gauge){
 if(!gameplayThread)gameplayThread=GetCurrentThreadId();
 if(InterlockedIncrement(&tickCalls)==1)note("hook hit: combat UI update");
 State*s=NULL;uintptr_t g=(uintptr_t)gauge;
 if(ready){uint32_t owner=u32(g+40);uintptr_t actor=resolve(owner,3);if(actor&&component(g,GAUGE_VT,owner,74)){uintptr_t info=infoFor(actor,owner);EnterCriticalSection(&stateLock);s=ensure(owner,info,g,0);if(s){s->refs++;apply(s,1);applyNewTraits(s);}LeaveCriticalSection(&stateLock);}}
 originalTick(gauge);
 if(ready){EnterCriticalSection(&stateLock);processExplosions();LeaveCriticalSection(&stateLock);}
 if(s){EnterCriticalSection(&stateLock);if(s->traits&&s->hpScaled&&!s->pendingHpBonus&&!s->hpUiLogged&&stillValid(s)){if(logFile){fprintf(logFile,"hp_after_ui owner=%08X hp=%lld max=%lld gauge_hp=%lld gauge_max=%lld\n",s->owner,(long long)i64(s->fighter+0xD20),(long long)i64(s->fighter+0xD18),(long long)i64(g+0x190),(long long)i64(g+0x198));fflush(logFile);}s->hpUiLogged=1;}LeaveCriticalSection(&stateLock);applyMotion(s);refreshGaugeName(s,g);EnterCriticalSection(&stateLock);s->refs--;LeaveCriticalSection(&stateLock);}
}
// The stock soldier setup at 0x8BFC71 resolves a name, constructs a game
// string with 0x5E64B0, then calls this gauge text setter at 0x1CC5A60.
// Calling that same setter avoids the character-name hook unused by this HUD.
static void refreshGaugeName(State*s,uintptr_t gauge){
 if((!s->traits&&!showHealthNumbers&&!s->labelGauge)||!stillValid(s)||!component(gauge,GAUGE_VT,s->owner,74))return;
 uint64_t handle=ptr(gauge+0x88);if(!handle)return;
 LARGE_INTEGER now;QueryPerformanceCounter(&now);
 EnterCriticalSection(&stateLock);int due=handle!=s->labelHandle||s->labelGauge!=u32(gauge+12)||!s->lastLabel||(double)(now.QuadPart-s->lastLabel)/qpcFrequency.QuadPart>=(showHealthNumbers?.10:1.);LeaveCriticalSection(&stateLock);if(!due)return;
 if(!s->label){uintptr_t name=0;originalName((void*)s->info,&name);if(!name)return;char base[512];size_t n=0;for(;n<sizeof(base)-1;n++){if(!readmem(name+n,base+n,1))return;if(base[n]==0)break;}base[n]=0;
  char label[1024];snprintf(label,sizeof(label),"%s%s%s%s%s%s%s%s%s%s%s",base,s->traits?" [":"",aff_elite(s->traits)?"<color=clan_battle_log_enemy>Elite</color> ":"",(s->traits&AFF_STRONG)?"<color=striking>Strong</color> ":"",(s->traits&AFF_SPEEDY)?"<color=batting_pitch_light_blue>Speedy</color> ":"",(s->traits&AFF_REGEN)?"<color=batting_pitch_green>Regen</color> ":"",(s->traits&AFF_BERSERK)?"<color=striking>Berserk</color> ":"",(s->traits&AFF_ENRAGED)?"<color=striking>Enraged</color> ":"",(s->traits&AFF_HEAVYWEIGHT)?"<color=batting_pitch_light_blue>Heavyweight</color> ":"",(s->traits&AFF_VOLATILE)?"<color=striking>Volatile</color>":"",s->traits?"]":"");
  size_t bytes=strlen(label)+1;char *copy=calloc(1,bytes+32);if(!copy)return;memcpy(copy,label,bytes);EnterCriticalSection(&stateLock);if(!s->label)s->label=copy;else free(copy);LeaveCriticalSection(&stateLock);
 }
 EnterCriticalSection(&stateLock);
 if(showHealthNumbers){char display[1280]={0};snprintf(display,sizeof(display),"%s  %lld/%lld HP",s->label,(long long)i64(s->fighter+0xD20),(long long)i64(s->fighter+0xD18));stringAssign(&s->uiString,display);}
 else stringAssign(&s->uiString,s->label);
 int first=!s->labelGauge;LeaveCriticalSection(&stateLock);
 if(!s->uiString||!component(gauge,GAUGE_VT,s->owner,74))return;
 setGaugeText((void*)gauge,&s->uiString);
 EnterCriticalSection(&stateLock);s->labelHandle=handle;s->labelGauge=u32(gauge+12);s->lastLabel=now.QuadPart;if(first&&logFile){fprintf(logFile,"label_set owner=%08X handle=%016llX text=%s\n",s->owner,(unsigned long long)handle,s->label);fflush(logFile);}LeaveCriticalSection(&stateLock);
}
static int itemRow(uint32_t row){return row>=252&&row<=260;}
static int moneyRow(uint32_t row){return row==9||row==10||(row>=233&&row<=251)||(row>=261&&row<=265);}
static void __fastcall hookRow(void *context,const void *position,uint32_t row){if(InterlockedIncrement(&rowCalls)==1)note("hook hit: reward row spawn");Scope*scope=TlsGetValue(tlsIndex);if(scope&&u32((uintptr_t)context+40)==scope->state->owner){if(itemRow(row))scope->item=1;if(moneyRow(row))scope->money=1;}originalRow(context,position,row);}
static void __fastcall hookCash(void *context,int32_t amount,const void *position,const void *origin){
 LONG calls=InterlockedIncrement(&cashCalls);Scope*scope=TlsGetValue(tlsIndex);uint32_t owner=u32((uintptr_t)context+40);
 if(scope&&owner==scope->state->owner&&scope->phase==1&&amount>0){amount=aff_reward_cash(amount,scope->baseReward);if(amount<=0)return;scope->bonusUnits+=amount;}
 if(calls<=128||scope){EnterCriticalSection(&stateLock);if(logFile){fprintf(logFile,"cash_spawn owner=%08X phase=%s internal_units=%d\n",owner,scope?(scope->phase==1?"trait_bonus":scope->phase==2?"fallback":"original"):"unscoped",amount);fflush(logFile);}LeaveCriticalSection(&stateLock);}
 originalCash(context,amount,position,origin);
}
static void __fastcall hookDefeat(void *context,const void *position){
 if(InterlockedIncrement(&defeatCalls)==1)note("hook hit: defeat reward");
 if(!ready){originalDefeat(context,position);return;}uintptr_t ctx=(uintptr_t)context;uint32_t owner=u32(ctx+40);uintptr_t actor=resolve(owner,3),info=actor?infoFor(actor,owner):0,g=actor?gaugeFor(actor,owner):0;
 EnterCriticalSection(&stateLock);State*s=ensure(owner,info,g,1);if(s&&s->paid)s=NULL;if(s){s->paid=1;s->refs++;}LeaveCriticalSection(&stateLock);if(!s){originalDefeat(context,position);return;}
 EnterCriticalSection(&stateLock);if((s->traits&AFF_VOLATILE)&&!s->explosionPending&&readmem((uintptr_t)position,s->explosionPosition,16)){s->explosionPending=1;s->explosionWait=volatileDelay;s->refs++;if(logFile){fprintf(logFile,"volatile_armed owner=%08X delay=%.3f\n",s->owner,volatileDelay);fflush(logFile);}}LeaveCriticalSection(&stateLock);
 EnterCriticalSection(&stateLock);double baseReward=aff_reward_total(s->traits,rewardValues),dropPercent=rewardDropChance;LeaveCriticalSection(&stateLock);
 int rewardPassed=dropPercent>=100;
 if(dropPercent>0&&dropPercent<100){uint32_t random;if(BCryptGenRandom(NULL,(PUCHAR)&random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0)rewardPassed=aff_reward_roll(dropPercent,random/4294967296.0);else note("Reward roll failed: RNG unavailable; original drops preserved.");}
 Scope scope={(Scope*)TlsGetValue(tlsIndex),s,0,0,0,0,baseReward};TlsSetValue(tlsIndex,&scope);originalDefeat(context,position);
 unsigned char pending=0;readmem(ctx+0x8C,&pending,1);uint32_t planned=u32(ctx+0x88);if(pending){if(itemRow(planned))scope.item=1;if(moneyRow(planned)||(planned==0&&i64(ctx+0x80)>0))scope.money=1;}
 int originalItem=scope.item,originalMoney=scope.money;scope.phase=1;if(rewardPassed&&baseReward>0)originalRow(context,position,BONUS_REWARD_ROW); // Resolve the installed table once; hookCash combines all trait bonuses into one pickup.
 scope.phase=2;
 if(scope.bonusUnits>0)scope.money=1;int guarantee=rewardPassed&&(aff_elite(s->traits)||s->boss);
 if(guarantee&&!scope.item)originalRow(context,position,252); // One existing level-1 medicine if the original roll supplied no item.
 if(guarantee&&!scope.money)originalRow(context,position,9); // Existing smallest active cash row; preserves installed table value.
 TlsSetValue(tlsIndex,scope.previous);if(logFile){EnterCriticalSection(&stateLock);fprintf(logFile,"defeat owner=%08X traits=%u base_bonus=%.2f reward_chance=%.2f reward_passed=%d guarantee=%d original_item=%d original_money=%d actual_bonus_units=%lld effective_table_multiplier=%.3f\n",owner,s->traits,baseReward,dropPercent,rewardPassed,guarantee,originalItem,originalMoney,(long long)scope.bonusUnits,baseReward>0&&rewardPassed?(double)scope.bonusUnits/(baseReward*100.):0.);fflush(logFile);LeaveCriticalSection(&stateLock);}EnterCriticalSection(&stateLock);s->refs--;LeaveCriticalSection(&stateLock);
}
#include "target_signatures.h"
#include "settings_panel.h"
static int fingerprint(void){char path[MAX_PATH];if(!GetModuleFileNameA(NULL,path,MAX_PATH))return 0;HANDLE f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);if(f==INVALID_HANDLE_VALUE)return 0;HCRYPTPROV provider=0;HCRYPTHASH hash=0;int ok=0;if(!CryptAcquireContextA(&provider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)||!CryptCreateHash(provider,CALG_MD5,0,0,&hash))goto done;BYTE buffer[65536],digest[16];DWORD count,size=16;while(ReadFile(f,buffer,sizeof(buffer),&count,NULL)&&count)if(!CryptHashData(hash,buffer,count,0))goto done;if(!CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0))goto done;const BYTE expected[]={0xD4,0xAA,0x28,0x9C,0xF4,0x29,0x4F,0x50,0x7B,0xF2,0x0D,0x3C,0xE2,0xFA,0xEB,0x16};ok=memcmp(digest,expected,16)==0;done:if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(f);return ok;}
static double rangedSetting(const char*key,double fallback,double low,double high){char text[64],def[64];snprintf(def,sizeof(def),"%.8f",fallback);GetPrivateProfileStringA("Affixes",key,def,text,sizeof(text),iniPath);char *end;double v=strtod(text,&end);while(*end==' '||*end=='\t')end++;return end!=text&&!*end&&isfinite(v)&&v>=low&&v<=high?v:fallback;}
static double setting(const char*key,double fallback){return rangedSetting(key,fallback,0,1);}
static DWORD WINAPI initialize(void *unused){(void)unused;char path[MAX_PATH];GetModuleFileNameA(selfModule,path,MAX_PATH);char *end=NULL;for(char *c=path;*c;c++)if(*c=='\\')end=c;if(end)*end=0;snprintf(iniPath,sizeof(iniPath),"%s\\PirateAffixes.ini",path);snprintf(logPath,sizeof(logPath),"%s\\PirateAffixes.log",path);logFile=fopen(logPath,"a");note("PirateAffixes 0.2.3 native prototype initializing. Runtime behavior requires in-game validation.");if(!fingerprint()){note("Disabled: executable MD5 differs from the supported Steam 1.14 build.");return 0;}
 image=(uintptr_t)GetModuleHandleA(NULL);InitializeCriticalSection(&stateLock);QueryPerformanceFrequency(&qpcFrequency);tlsIndex=TlsAlloc();if(tlsIndex==TLS_OUT_OF_INDEXES){note("Disabled: TLS allocation failed.");return 0;}
 chances[0]=setting("StrongChance",.2);chances[1]=setting("SpeedyChance",.2);chances[2]=setting("RegenChance",.2);chances[3]=setting("BerserkChance",.10);chances[4]=setting("EnragedChance",.10);chances[5]=setting("HeavyweightChance",.10);chances[6]=setting("VolatileChance",.08);regenRate=setting("RegenPerSecond",.03);affixHp=rangedSetting("AffixHPMultiplier",2.5,1,100);strongHp=rangedSetting("StrongHPMultiplier",6,1,100);strongPower=rangedSetting("StrongDamageMultiplier",3,.1,10);speedScale=rangedSetting("SpeedyAnimationMultiplier",3.5,.1,10);enragedMaxSpeed=rangedSetting("EnragedMaxAnimationMultiplier",4,1,5);enragedIncoming=rangedSetting("EnragedMaxIncomingDamageMultiplier",1.5,1,5);heavySpeed=rangedSetting("HeavyweightAnimationMultiplier",.5,.1,1);volatileDelay=rangedSetting("VolatileDelaySeconds",1.5,.25,10);volatileRadius=rangedSetting("VolatileRadius",5,1,10);volatileDamage=rangedSetting("VolatileDamageMultiplier",5,.1,5);volatileBaseDamage=rangedSetting("VolatileBaseDamageMultiplier",5,.1,20);heavyHp=rangedSetting("HeavyweightHPMultiplier",6,1,100);heavyPower=rangedSetting("HeavyweightDamageMultiplier",1.5,.1,10);berserkPower=rangedSetting("BerserkDamageMultiplier",2,1,10);berserkThreshold=rangedSetting("BerserkHealthThreshold",.5,.01,1);showHealthNumbers=(int)rangedSetting("ShowHealthNumbers",1,0,1);
 for(unsigned i=0;i<AFF_TRAIT_COUNT;i++)rewardValues[i]=rangedSetting(rewardKeys[i],500,0,100000);
 rewardDropChance=rangedSetting("TraitRewardDropChance",100,0,100);
 if(logFile){fprintf(logFile,"reward_settings chance=%.2f values=%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",rewardDropChance,rewardValues[0],rewardValues[1],rewardValues[2],rewardValues[3],rewardValues[4],rewardValues[5],rewardValues[6]);fflush(logFile);}
 if(logFile){fprintf(logFile,"settings affix_hp=%.3f strong_hp=%.3f strong_power=%.3f speedy_animation=%.3f regen=%.5f hp_numbers=%d\n",affixHp,strongHp,strongPower,speedScale,regenRate,showHealthNumbers);fflush(logFile);}
 if(logFile){fprintf(logFile,"new_trait_settings enraged_chance=%.3f heavyweight_chance=%.3f volatile_chance=%.3f enraged_speed=%.3f enraged_incoming=%.3f heavy_speed=%.3f volatile_delay=%.3f volatile_radius=%.3f volatile_damage=%.3f\n",chances[4],chances[5],chances[6],enragedMaxSpeed,enragedIncoming,heavySpeed,volatileDelay,volatileRadius,volatileDamage);fflush(logFile);}
 if(logFile){fprintf(logFile,"balance_021 heavyweight_hp=%.3f heavyweight_power=%.3f volatile_base=%.3f volatile_user=%.3f\n",heavyHp,heavyPower,volatileBaseDamage,volatileDamage);fflush(logFile);}
 if(logFile){fprintf(logFile,"berserk_settings chance=%.3f damage=%.3f threshold=%.3f\n",chances[3],berserkPower,berserkThreshold);fflush(logFile);}
 createBomb=(CreateBombFn)(image+0x5DFD50);getInfo=(GetInfoFn)(image+0x6750A0);originalName=(NameFn)(image+0x8BE080);stringAssign=(StringAssignFn)(image+0x5E64B0);setGaugeText=(GaugeTextFn)(image+0x1CC5A60);
 unsigned char expectedName[]={0x40,0x53,0x48,0x83,0xEC,0x20},actual[6];if(!readmem(image+0x8BE080,actual,6)||memcmp(actual,expectedName,6)){note("Disabled: name function bytes differ.");return 0;}
 if(!validateTargets())return 0;
 if(MH_Initialize()!=MH_OK||MH_CreateHook((void*)(image+0x163B590),hookMotionSpeed,(void**)&originalMotionSpeed)!=MH_OK||MH_CreateHook((void*)(image+0x1CC8B10),hookTick,(void**)&originalTick)!=MH_OK||MH_CreateHook((void*)(image+0x8DE7A0),hookDefeat,(void**)&originalDefeat)!=MH_OK||MH_CreateHook((void*)(image+0x8DED80),hookRow,(void**)&originalRow)!=MH_OK||MH_CreateHook((void*)(image+0x8DF800),hookCash,(void**)&originalCash)!=MH_OK){note("Disabled: hook creation failed; no hooks enabled.");return 0;}
 InterlockedExchange(&ready,1);if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){InterlockedExchange(&ready,0);MH_DisableHook(MH_ALL_HOOKS);note("Disabled: enabling hooks failed.");return 0;}note("Hooks enabled: fresh per-actor rolls, Strong/Speedy/Regen/Berserk/Enraged/Heavyweight/Volatile, name labels and defeat rewards. Await in-game validation.");HANDLE panel=CreateThread(NULL,0,settingsPanelThread,NULL,0,NULL);if(panel)CloseHandle(panel);return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){(void)reserved;if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(NULL,0,initialize,NULL,0,NULL);if(thread)CloseHandle(thread);}return TRUE;}
