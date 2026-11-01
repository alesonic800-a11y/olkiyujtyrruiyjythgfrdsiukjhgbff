#include <3ds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sushi_sprite.h"
#include "assets/sushi_assets.h"
#include "world_backgrounds.h"

#define SW 400
#define SH 240
#define WORLD_W 2400
#define MAX_STARS 18
#define MAX_PLATFORMS 18
#define AUDIO_SAMPLES 4096
#define PLAYER_CW 28
#define PLAYER_CH 30

typedef enum { MODE_TITLE, MODE_PLAY, MODE_PAUSE, MODE_CLEAR, MODE_OVER, MODE_CAMPAIGN } Mode;
typedef enum { FX_JUMP, FX_STAR, FX_HURT, FX_MEOW, FX_BOSS, FX_CLEAR } Sfx;
typedef struct { int x,y; bool taken; } Star;
typedef struct { int x,y,w,h,tile; } Platform;
typedef struct { int x,y,dir,type,alive; } Enemy;
typedef struct { const char *name,*boss; uint32_t accent,enemy; int bpm; } Theme;
static const Theme themes[6]={
 {"MOONLIT GARDEN","Night Prowler",0xD8E8FF,0xD8D8E8,118}, {"TROPICAL ISLAND","King Coconut",0xFFE16B,0xD76B35,132},
 {"UNDERWATER CITY","Abyssal Eel",0x8CF4F0,0x58C8D8,104}, {"WILD FRONTIER","Rumble Mammoth",0xF7D77B,0x8A6B4C,124},
 {"GALACTIC NEBULA","Void Dragon",0x7BF7FF,0xB78CFF,148}, {"LAVA FORTRESS","Inferno King",0xFFD34E,0xC6A09A,160}
};
static Mode mode=MODE_TITLE;
static int level=0,frame=0,score=0,lives=3,hurtTimer=0,camera=0,sliderSep=0,bossHp=0,bossX=0,bossDir=1,musicTick=0,musicStep=0,anim=0;
static float px=42,py=150,vx=0,vy=0;
static bool grounded=false,duck=false,meow=false,bossActive=false,faceLeft=false;
static Star stars[MAX_STARS]; static Enemy enemies[6]; static Platform platforms[MAX_PLATFORMS]; static int platformCount=0;
static uint8_t *leftFb,*rightFb; static ndspWaveBuf audioWave; static int8_t audioPcm[AUDIO_SAMPLES]; static bool audioReady=false;
static inline void pixel(uint8_t *fb,int x,int y,uint32_t rgb){if(!fb||x<0||x>=SW||y<0||y>=SH)return;unsigned i=((unsigned)x*SH+(unsigned)y)*3;fb[i]=(uint8_t)rgb;fb[i+1]=(uint8_t)(rgb>>8);fb[i+2]=(uint8_t)(rgb>>16);}
static void rect(uint8_t *fb,int x,int y,int w,int h,uint32_t c){for(int xx=x;xx<x+w;xx++)for(int yy=y;yy<y+h;yy++)pixel(fb,xx,yy,c);}
static void circle(uint8_t *fb,int cx,int cy,int r,uint32_t c){for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++)if(x*x+y*y<=r*r)pixel(fb,cx+x,cy+y,c);}
static void line(uint8_t *fb,int x0,int y0,int x1,int y1,uint32_t c){int dx=x1-x0,dy=y1-y0,n=abs(dx)>abs(dy)?abs(dx):abs(dy);if(!n){pixel(fb,x0,y0,c);return;}for(int i=0;i<=n;i++)pixel(fb,x0+dx*i/n,y0+dy*i/n,c);}
static void draw_img(uint8_t *fb,int x,int y,const uint32_t *data,int w,int h,bool flip){for(int sy=0;sy<h;sy++)for(int sx=0;sx<w;sx++){uint32_t c=data[sy*w+(flip?w-1-sx:sx)];if(c)pixel(fb,x+sx,y+sy,c);}}
static void draw_bg(uint8_t *fb,int eye){const uint16_t *d=world_bg_0;switch(level){case 1:d=world_bg_1;break;case 2:d=world_bg_2;break;case 3:d=world_bg_3;break;case 4:d=world_bg_4;break;case 5:d=world_bg_5;break;}int shift=(camera/4-eye/2)%BG_W;if(shift<0)shift+=BG_W;for(int x=0;x<SW;x++){int sx=(x+shift)%BG_W;for(int y=0;y<SH;y++){uint16_t c=d[y*BG_W+sx];pixel(fb,x,y,((uint32_t)((c>>11)&31)*255/31<<16)|((uint32_t)((c>>5)&63)*255/63<<8)|((uint32_t)(c&31)*255/31));}}}
static void audio_note(float hz,int n,float vol){if(!audioReady||audioWave.status==NDSP_WBUF_PLAYING)return;if(n>AUDIO_SAMPLES)n=AUDIO_SAMPLES;for(int i=0;i<n;i++){float e=1.0f-(float)i/(float)n;audioPcm[i]=(int8_t)(sinf(6.2831853f*hz*i/22050.0f)*vol*110.0f*e);}audioWave.nsamples=n;audioWave.looping=false;audioWave.status=NDSP_WBUF_FREE;DSP_FlushDataCache(audioPcm,n);ndspChnWaveBufAdd(0,&audioWave);}
static void sfx(Sfx f){static const float hz[]={660,1046,150,880,95,1320};static const int ns[]={900,700,1800,1200,3000,2500};audio_note(hz[f],ns[f],0.55f);}
static void music_update(void){if(!audioReady||mode!=MODE_PLAY)return;if(++musicTick<(60*60)/(themes[level].bpm*2))return;musicTick=0;static const float notes[]={1,1.12246f,1.25992f,1.49831f,1.25992f,1.12246f,1.33484f,1.49831f};static const int base[]={220,262,196,294,165,147};audio_note(base[level]*notes[musicStep++%8],2200,0.18f);}
static void add_platform(int x,int y,int w,int tile){if(platformCount<MAX_PLATFORMS)platforms[platformCount++]=(Platform){x,y,w,14,tile%TILE_COUNT};}
static void init_level(int n){level=n;score=0;lives=3;hurtTimer=0;px=42;py=140;vx=vy=0;camera=0;grounded=false;bossActive=false;bossHp=0;bossX=WORLD_W-150;bossDir=-1;musicTick=musicStep=frame=0;platformCount=0;
 /* Real multi-height platform layout. Gaps are intentional pits; every route has a reachable stepping path. */
 add_platform(0,202,360,level%8);add_platform(400,202,310,(level+1)%8);add_platform(750,202,390,(level+2)%8);add_platform(1190,202,330,(level+3)%8);add_platform(1570,202,340,(level+4)%8);add_platform(1950,202,450,(level+5)%8);
 add_platform(155,164,92,0);add_platform(290,137,88,1);add_platform(475,165,100,2);add_platform(620,143,90,3);add_platform(850,166,105,4);add_platform(1010,137,92,5);add_platform(1260,164,100,6);add_platform(1435,137,92,7);add_platform(1640,164,100,0);add_platform(1810,138,100,1);add_platform(2070,164,110,2);add_platform(2225,136,100,3);
 for(int i=0;i<MAX_STARS;i++){stars[i].x=115+i*126;stars[i].y=(i%3==0)?112:((i%3==1)?83:151);stars[i].taken=false;}
 for(int i=0;i<6;i++){int ex=270+i*330;int ey=202;for(int j=0;j<platformCount;j++)if(ex>=platforms[j].x&&ex<platforms[j].x+platforms[j].w&&platforms[j].y<ey)ey=platforms[j].y;enemies[i]=(Enemy){ex,ey-ENEMY_H,((i+level)%2)?-1:1,(i+level)%ENEMY_COUNT,1};}}
static void draw_platforms(uint8_t *fb,int eye){const uint32_t *ts[]={tile_0,tile_1,tile_2,tile_3,tile_4,tile_5,tile_6,tile_7};for(int i=0;i<platformCount;i++){Platform p=platforms[i];int x=p.x-camera+eye;for(int k=0;k<p.w;k+=TILE_W){int sx=x+k;if(sx>-TILE_W&&sx<SW)draw_img(fb,sx,p.y,ts[p.tile],TILE_W,TILE_H,false);}}}
static void draw_player(uint8_t *fb,int x,int y){int f;if(!grounded)f=duck?10:8;else if(duck)f=11;else if(fabsf(vx)>0.4f)f=(anim/8)%4;else f=3+(anim/24)%2;const uint32_t *a[]={player_0,player_1,player_2,player_3,player_4,player_5,player_6,player_7,player_8,player_9,player_10,player_11};draw_img(fb,x,y,a[f],PLAYER_W,PLAYER_H,faceLeft);if(meow&&frame%12<6){circle(fb,x+34,y+9,4,0xFFFFFF);circle(fb,x+43,y+3,3,0xFFFFFF);}}
static void draw_enemy(uint8_t *fb,int x,int y,int type){const uint32_t *a[]={enemy_0,enemy_1,enemy_2,enemy_3,enemy_4,enemy_5,enemy_6,enemy_7};draw_img(fb,x,y,a[type%ENEMY_COUNT],ENEMY_W,ENEMY_H,false);}
static void draw_boss(uint8_t *fb,int eye){const uint32_t *a[]={boss_0,boss_1,boss_2,boss_3,boss_4,boss_5};draw_img(fb,bossX-camera+eye,135,a[level],BOSS_W,BOSS_H,false);rect(fb,135,12,130,8,0x281010);rect(fb,137,14,bossHp*126/8,4,0xF04444);}
static void draw_world(uint8_t *fb,int eye){draw_bg(fb,eye);draw_platforms(fb,eye);for(int i=0;i<MAX_STARS;i++)if(!stars[i].taken){int x=stars[i].x-camera+eye;circle(fb,x,stars[i].y,6,0xFFD52E);line(fb,x-8,stars[i].y,x+8,stars[i].y,0xFFFFFF);}for(int i=0;i<6;i++)if(enemies[i].alive){int x=enemies[i].x-camera+eye;if(x>-ENEMY_W&&x<SW)draw_enemy(fb,x,enemies[i].y,enemies[i].type);}if(bossActive)draw_boss(fb,eye);draw_player(fb,(int)px-camera+eye,(int)py);}
static void draw_title(uint8_t *fb,int eye){draw_bg(fb,eye);draw_img(fb,180+eye,90,boss_0,BOSS_W,BOSS_H,false);rect(fb,75,178,250,4,themes[level].accent);}
static void draw_top(void){u16 w,h;leftFb=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&w,&h);rightFb=gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,&w,&h);if(mode==MODE_TITLE||mode==MODE_CAMPAIGN){draw_title(leftFb,-sliderSep);draw_title(rightFb,sliderSep);}else{draw_world(leftFb,-sliderSep);draw_world(rightFb,sliderSep);}gfxFlushBuffers();gfxSwapBuffers();}
static bool overlap(float ax,float ay,float aw,float ah,int bx,int by,int bw,int bh){return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;}
static void hurt(void){if(!hurtTimer){lives--;hurtTimer=90;vx=-2.0f;vy=-3.2f;sfx(FX_HURT);if(lives<=0)mode=MODE_OVER;}}
static void update_game(u32 held,u32 down){if(down&KEY_START){mode=MODE_PAUSE;return;}if(held&KEY_LEFT){vx=-2.0f;faceLeft=true;}else if(held&KEY_RIGHT){vx=2.0f;faceLeft=false;}else vx*=0.78f;duck=(held&KEY_DOWN)!=0;meow=(held&KEY_UP)!=0;if((down&KEY_UP)&&meow)sfx(FX_MEOW);if((down&KEY_A)&&grounded){vy=-6.6f;grounded=false;sfx(FX_JUMP);}float oldY=py;vy+=0.28f;if(vy>6.5f)vy=6.5f;px+=vx;if(px<0)px=0;if(px>WORLD_W-PLAYER_CW)px=WORLD_W-PLAYER_CW;py+=vy;grounded=false;
 /* One-way platform collision: land only while descending from above. */
 if(vy>=0){for(int i=0;i<platformCount;i++){Platform p=platforms[i];if(px+PLAYER_CW>p.x&&px<p.x+p.w&&oldY+PLAYER_CH<=p.y+3&&py+PLAYER_CH>=p.y){py=p.y-PLAYER_CH;vy=0;grounded=true;break;}}}if(py>SH+35){hurt();px=(float)(px<400?42:((int)px/400)*400-55);py=110;vy=0;}
 if(hurtTimer>0)hurtTimer--;
 anim++;
 for(int i=0;i<6;i++)if(enemies[i].alive){Enemy *e=&enemies[i];e->x+=e->dir;if(e->x<camera-20||e->x>camera+SW+100)e->dir=-e->dir;if(overlap(px,py,PLAYER_CW,PLAYER_CH,e->x,e->y,ENEMY_W,ENEMY_H)){if(vy>1.0f&&py+PLAYER_CH<e->y+15){e->alive=0;vy=-4.2f;score+=25;sfx(FX_STAR);}else hurt();}}
 for(int i=0;i<MAX_STARS;i++)if(!stars[i].taken&&overlap(px,py,PLAYER_CW,PLAYER_CH,stars[i].x-7,stars[i].y-7,14,14)){stars[i].taken=true;score+=10;sfx(FX_STAR);}
 camera=(int)px-150;if(camera<0)camera=0;if(camera>WORLD_W-SW)camera=WORLD_W-SW;
 if(px>WORLD_W-360&&!bossActive){bossActive=true;bossHp=8;bossX=WORLD_W-150;sfx(FX_BOSS);}if(bossActive){bossX+=bossDir;if(bossX<WORLD_W-290||bossX>WORLD_W-75)bossDir=-bossDir;if(overlap(px,py,PLAYER_CW,PLAYER_CH,bossX,145,BOSS_W,BOSS_H)){if(vy>0&&py+PLAYER_CH<174){bossHp--;vy=-5;sfx(FX_BOSS);}else hurt();}if((down&KEY_UP)&&abs((int)px-bossX)<95){bossHp--;sfx(FX_MEOW);}if(bossHp<=0){bossActive=false;mode=MODE_CLEAR;sfx(FX_CLEAR);}}frame++;music_update();}
int main(void){gfxInitDefault();gfxSet3D(true);consoleInit(GFX_BOTTOM,NULL);if(ndspInit()==0){audioReady=true;ndspSetOutputMode(NDSP_OUTPUT_STEREO);ndspChnReset(0);ndspChnSetInterp(0,NDSP_INTERP_LINEAR);ndspChnSetRate(0,22050.0f);ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM8);memset(&audioWave,0,sizeof(audioWave));audioWave.data_vaddr=audioPcm;}init_level(0);
 while(aptMainLoop()){hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();sliderSep=(int)(osGet3DSliderState()*5.0f);if(mode==MODE_TITLE){if(down&KEY_LEFT)level=(level+5)%6;if(down&KEY_RIGHT)level=(level+1)%6;if(down&KEY_A){init_level(level);mode=MODE_PLAY;}if(down&KEY_B)break;}else if(mode==MODE_PLAY)update_game(held,down);else if(mode==MODE_PAUSE){if(down&KEY_A)mode=MODE_PLAY;if(down&(KEY_B|KEY_SELECT))mode=MODE_TITLE;}else if(mode==MODE_CLEAR){if(down&KEY_A){if(level<5){init_level(level+1);mode=MODE_PLAY;}else mode=MODE_CAMPAIGN;}if(down&KEY_B)mode=MODE_TITLE;}else if(mode==MODE_OVER){if(down&KEY_A)mode=MODE_TITLE;if(down&KEY_B)break;}else if(mode==MODE_CAMPAIGN){if(down&KEY_A)mode=MODE_TITLE;if(down&KEY_B)break;}draw_top();consoleClear();printf("SUSHI THE CAT - 3DS\n");if(mode==MODE_TITLE)printf("\nWORLD SELECT\n\n%s\nBOSS: %s\n\nLEFT/RIGHT: WORLD\nA: PLAY  B: EXIT\n3D SLIDER: STEREO",themes[level].name,themes[level].boss);else if(mode==MODE_PLAY)printf("\n%s\nSCORE %d  LIVES %d\n\nLEFT/RIGHT MOVE\nA JUMP  UP MEOW\nDOWN DUCK  START PAUSE\nBOSS: %s",themes[level].name,score,lives,bossActive?"ACTIVE":"APPROACHING");else if(mode==MODE_PAUSE)printf("\nPAUSED\nA CONTINUE\nB/SELECT TITLE");else if(mode==MODE_CLEAR)printf("\nBOSS DEFEATED!\n%s\nSCORE %d\nA NEXT WORLD  B MENU",themes[level].name,score);else if(mode==MODE_CAMPAIGN)printf("\nCAMPAIGN COMPLETE!\nA MENU  B EXIT");else printf("\nGAME OVER\nSCORE %d\nA MENU  B EXIT",score);gspWaitForVBlank();}
 if(audioReady)ndspExit();
 gfxExit();return 0;}
