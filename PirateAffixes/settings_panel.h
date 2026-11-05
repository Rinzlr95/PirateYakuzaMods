#ifndef AFFIX_SETTINGS_PANEL_H
#define AFFIX_SETTINGS_PANEL_H
#include <commctrl.h>
// Native Win32 panel: no graphics-device injection. F8 opens it while the
// game is focused; F7 toggles health numbers. Borderless/windowed recommended.
typedef struct PanelSetting {const char *key;double *value,low,high,step;int page;} PanelSetting;
static PanelSetting panelSettings[]={
 {"TraitRewardDropChance",&rewardDropChance,0,100,1,0},
 {"StrongReward",&rewardValues[0],0,100000,25,0},{"SpeedyReward",&rewardValues[1],0,100000,25,0},
 {"RegenReward",&rewardValues[2],0,100000,25,0},{"BerserkReward",&rewardValues[3],0,100000,25,0},
 {"EnragedReward",&rewardValues[4],0,100000,25,0},{"HeavyweightReward",&rewardValues[5],0,100000,25,0},
 {"VolatileReward",&rewardValues[6],0,100000,25,0},
 {"StrongChance",&chances[0],0,1,.01,1},{"SpeedyChance",&chances[1],0,1,.01,2},
 {"RegenChance",&chances[2],0,1,.01,3},{"BerserkChance",&chances[3],0,1,.01,4},
 {"EnragedChance",&chances[4],0,1,.01,5},{"HeavyweightChance",&chances[5],0,1,.01,6},
 {"VolatileChance",&chances[6],0,1,.01,7},
 {"RegenPerSecond",&regenRate,0,1,.01,3},{"AffixHPMultiplier",&affixHp,1,100,.1,0},
 {"StrongHPMultiplier",&strongHp,1,100,.1,1},{"StrongDamageMultiplier",&strongPower,.1,10,.1,1},
 {"SpeedyAnimationMultiplier",&speedScale,.1,10,.1,2},{"BerserkDamageMultiplier",&berserkPower,1,10,.1,4},
 {"BerserkHealthThreshold",&berserkThreshold,.01,1,.01,4},
 {"EnragedMaxAnimationMultiplier",&enragedMaxSpeed,1,5,.1,5},
 {"EnragedMaxIncomingDamageMultiplier",&enragedIncoming,1,5,.1,5},
 {"HeavyweightAnimationMultiplier",&heavySpeed,.1,1,.05,6},
 {"VolatileDelaySeconds",&volatileDelay,.25,10,.25,7},
 {"HeavyweightHPMultiplier",&heavyHp,1,100,.1,6},{"HeavyweightDamageMultiplier",&heavyPower,.1,10,.1,6},
 {"VolatileBaseDamageMultiplier",&volatileBaseDamage,.1,20,.1,7},{"VolatileRadius",&volatileRadius,1,10,.25,7},{"VolatileDamageMultiplier",&volatileDamage,.1,5,.1,7}
};
enum { PANEL_ROWS=5, PANEL_PAGES=8 };
static const char *panelPageNames[PANEL_PAGES]={"General","Strong","Speedy","Regen","Berserk","Enraged","Heavyweight","Volatile"};
static HWND panelWindow,panelSliders[PANEL_ROWS],panelLabels[PANEL_ROWS],panelHealth,panelButtons[PANEL_PAGES];
static int panelPage,panelOffset,panelMap[PANEL_ROWS];
static void savePanelSetting(const char *key,double value){char text[64];snprintf(text,sizeof(text),"%.4f",value);if(!WritePrivateProfileStringA("Affixes",key,text,iniPath))note("Settings save failed: check INI folder write access.");}
static void refreshPanel(void){
 EnterCriticalSection(&stateLock);int row=0,total=0,skip=0;
 for(unsigned i=0;i<sizeof(panelSettings)/sizeof(panelSettings[0]);i++)if(panelSettings[i].page==panelPage)total++;
 int maximum=total>PANEL_ROWS?total-PANEL_ROWS:0;if(panelOffset>maximum)panelOffset=maximum;if(panelOffset<0)panelOffset=0;
 SCROLLINFO scroll={0};scroll.cbSize=sizeof(scroll);scroll.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;scroll.nMin=0;scroll.nMax=total-1;scroll.nPage=PANEL_ROWS;scroll.nPos=panelOffset;SetScrollInfo(panelWindow,SB_VERT,&scroll,TRUE);ShowScrollBar(panelWindow,SB_VERT,total>PANEL_ROWS);

 for(int page=0;page<PANEL_PAGES;page++)SendMessageA(panelButtons[page],BM_SETSTYLE,page==panelPage?BS_DEFPUSHBUTTON:BS_PUSHBUTTON,TRUE);
 char title[128];snprintf(title,sizeof(title),"PirateAffixes - %s - F8 settings / F7 HP",panelPageNames[panelPage]);SetWindowTextA(panelWindow,title);
 for(unsigned i=0;i<sizeof(panelSettings)/sizeof(panelSettings[0]);i++)if(panelSettings[i].page==panelPage&&row<PANEL_ROWS){
  if(skip++<panelOffset)continue;
  PanelSetting*p=&panelSettings[i];panelMap[row]=(int)i;char label[128];snprintf(label,sizeof(label),"%s = %.2f",p->key,*p->value);
  SetWindowTextA(panelLabels[row],label);SendMessageA(panelSliders[row],TBM_SETRANGEMIN,FALSE,0);
  SendMessageA(panelSliders[row],TBM_SETRANGEMAX,FALSE,(LPARAM)lround((p->high-p->low)/p->step));
  SendMessageA(panelSliders[row],TBM_SETPOS,TRUE,(LPARAM)lround((*p->value-p->low)/p->step));
  ShowWindow(panelLabels[row],SW_SHOW);ShowWindow(panelSliders[row],SW_SHOW);row++;
 }
 for(;row<PANEL_ROWS;row++){panelMap[row]=-1;ShowWindow(panelLabels[row],SW_HIDE);ShowWindow(panelSliders[row],SW_HIDE);}
 SendMessageA(panelHealth,BM_SETCHECK,showHealthNumbers?BST_CHECKED:BST_UNCHECKED,0);LeaveCriticalSection(&stateLock);
}
static void toggleHealthNumbers(void){EnterCriticalSection(&stateLock);showHealthNumbers=!showHealthNumbers;for(State*s=states;s;s=s->next)s->lastLabel=0;int value=showHealthNumbers;LeaveCriticalSection(&stateLock);savePanelSetting("ShowHealthNumbers",value);if(panelHealth)SendMessageA(panelHealth,BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);}
static LRESULT CALLBACK panelProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 switch(message){
 case WM_CLOSE:ShowWindow(window,SW_HIDE);return 0;
 case WM_COMMAND:
  if(LOWORD(wp)>=100&&LOWORD(wp)<100+PANEL_PAGES){panelPage=LOWORD(wp)-100;panelOffset=0;refreshPanel();return 0;}
  if(LOWORD(wp)==200){toggleHealthNumbers();return 0;}break;
 case WM_VSCROLL:{SCROLLINFO scroll={0};scroll.cbSize=sizeof(scroll);scroll.fMask=SIF_ALL;GetScrollInfo(window,SB_VERT,&scroll);
  switch(LOWORD(wp)){case SB_LINEUP:panelOffset--;break;case SB_LINEDOWN:panelOffset++;break;case SB_PAGEUP:panelOffset-=PANEL_ROWS;break;case SB_PAGEDOWN:panelOffset+=PANEL_ROWS;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:panelOffset=scroll.nTrackPos;break;case SB_TOP:panelOffset=0;break;case SB_BOTTOM:panelOffset=scroll.nMax;break;default:return 0;}refreshPanel();return 0;}
 case WM_MOUSEWHEEL:panelOffset-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA;refreshPanel();return 0;
 case WM_HSCROLL:
  for(int row=0;row<PANEL_ROWS;row++)if((HWND)lp==panelSliders[row]&&panelMap[row]>=0){
   PanelSetting*p=&panelSettings[panelMap[row]];LRESULT position=SendMessageA((HWND)lp,TBM_GETPOS,0,0);
   double value=fmin(p->high,fmax(p->low,p->low+position*p->step));
   EnterCriticalSection(&stateLock);*p->value=value;LeaveCriticalSection(&stateLock);savePanelSetting(p->key,value);
   char label[128];snprintf(label,sizeof(label),"%s = %.2f",p->key,value);SetWindowTextA(panelLabels[row],label);return 0;
  }
  break;
 case WM_TIMER:{static int wasF7,wasF8;int f7=(GetAsyncKeyState(VK_F7)&0x8000)!=0,f8=(GetAsyncKeyState(VK_F8)&0x8000)!=0;DWORD process=0;HWND foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&process);
  if(process==GetCurrentProcessId()){
   if(f7&&!wasF7)toggleHealthNumbers();
   if(f8&&!wasF8){if(IsWindowVisible(window))ShowWindow(window,SW_HIDE);else{refreshPanel();ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}}
  }wasF7=f7;wasF8=f8;return 0;}
 }return DefWindowProcA(window,message,wp,lp);
}
static DWORD WINAPI settingsPanelThread(void*unused){(void)unused;
 INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&controls)){note("Settings panel disabled: common controls unavailable.");return 0;}
 WNDCLASSA cls={0};cls.lpfnWndProc=panelProc;cls.hInstance=selfModule;cls.lpszClassName="PirateAffixesSettingsPanel";cls.hCursor=LoadCursorA(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
 if(!RegisterClassA(&cls)){note("Settings panel disabled: window registration failed.");return 0;}
 RECT work={0};MONITORINFO monitor={0};monitor.cbSize=sizeof(monitor);
 if(GetMonitorInfoA(MonitorFromWindow(GetForegroundWindow(),MONITOR_DEFAULTTOPRIMARY),&monitor))work=monitor.rcWork;else SystemParametersInfoA(SPI_GETWORKAREA,0,&work,0);
 int width=650,height=720,available=work.bottom-work.top;if(height>available-24)height=available-24;
 int x=work.left+80,y=work.top+120;if(y+height>work.bottom-12)y=work.bottom-height-12;if(y<work.top)y=work.top;
 panelWindow=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,"PirateAffixes - F8 settings / F7 HP numbers",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,x,y,width,height,NULL,NULL,selfModule,NULL);
 if(!panelWindow){note("Settings panel disabled: window creation failed.");return 0;}
 HWND w=panelWindow;
 CreateWindowA("STATIC","Changes save immediately. Chances and HP scales affect NEW enemies.\nOther settings update live. F8 hides this panel; F7 toggles HP numbers.\nGeneral scrolls for rewards. This panel does not pause the game.",WS_CHILD|WS_VISIBLE,14,12,610,62,w,NULL,selfModule,NULL);
 for(int page=0;page<PANEL_PAGES;page++)panelButtons[page]=CreateWindowA("BUTTON",panelPageNames[page],WS_CHILD|WS_VISIBLE|WS_TABSTOP,14+(page%4)*155,80+(page/4)*38,148,32,w,(HMENU)(INT_PTR)(100+page),selfModule,NULL);
 panelHealth=CreateWindowA("BUTTON","Show current/max HP numbers (F7)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,14,160,610,30,w,(HMENU)200,selfModule,NULL);
 for(int row=0;row<PANEL_ROWS;row++){
  panelLabels[row]=CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE,14,210+row*78,610,24,w,NULL,selfModule,NULL);
  panelSliders[row]=CreateWindowA(TRACKBAR_CLASSA,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_AUTOTICKS,14,236+row*78,605,36,w,NULL,selfModule,NULL);
 }
 refreshPanel();SetTimer(w,1,50,NULL);note("Settings ready: F8 native panel, F7 HP numbers. Game focus required.");
 MSG msg;while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return 0;
}
#endif
