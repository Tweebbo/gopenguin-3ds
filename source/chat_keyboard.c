#include "chat_keyboard.h"
#include <stdio.h>
#include <string.h>
static const char *const letters[4][10]={
 {"1","2","3","4","5","6","7","8","9","0"},
 {"q","w","e","r","t","y","u","i","o","p"},
 {"a","s","d","f","g","h","j","k","l","'"},
 {"z","x","c","v","b","n","m",",",".","?"}
};
static const char *const symbols[4][10]={
 {"!","@","#","&","*","(",")","-","_","+"},
 {"á","é","í","ó","ú","ñ","ü","ç","¿","¡"},
 {"Á","É","Í","Ó","Ú","Ñ",":",";","\"","/"},
 {"=","%","$","<",">","[","]","~","^","|"}
};
static const int actionX[]={0,48,96,216,268},actionW[]={46,46,118,50,50};
static unsigned characters(const char *text){unsigned n=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++)if((*p&0xc0)!=0x80)n++;return n;}
static const char *letter(const ChatKeyboard *k,unsigned row,unsigned col,char out[2]){
 const char *value=(k->symbols?symbols:letters)[row][col];
 if(!k->symbols&&(k->shift||k->caps)&&value[0]>='a'&&value[0]<='z'&&!value[1]){out[0]=value[0]-32;out[1]=0;return out;}return value;
}
static void append(ChatKeyboard *k,const char *text){
 size_t before=strlen(k->text),extra=strlen(text);if(characters(k->text)>=CHAT_KEYBOARD_CHARS||before+extra>=sizeof k->text)return;
 memcpy(k->text+before,text,extra+1);k->error[0]=0;if(k->shift&&!k->caps)k->shift=false;
}
static void backspace(ChatKeyboard *k){size_t n=strlen(k->text);if(!n)return;n--;while(n&&((unsigned char)k->text[n]&0xc0)==0x80)n--;k->text[n]=0;k->error[0]=0;}
static void shift(ChatKeyboard *k){if(k->caps)k->caps=k->shift=false;else if(k->shift)k->caps=true;else k->shift=true;}
static void send(ChatKeyboard *k){
 bool blank=true;for(const char *p=k->text;*p;p++)if(*p!=' ')blank=false;if(blank)return;
 if(!k->send||!k->send(k->text,k->user)){snprintf(k->error,sizeof k->error,"Not connected. Your message is kept.");return;}
 chat_keyboard_cancel(k);
}
static void activate(ChatKeyboard *k,int key){
 if(key>=0&&key<40){char out[2];append(k,letter(k,(unsigned)key/10,(unsigned)key%10,out));}
 else if(key==40)shift(k);else if(key==41)k->symbols=!k->symbols;else if(key==42)append(k," ");else if(key==43)backspace(k);else if(key==44)send(k);else if(key==45)chat_keyboard_cancel(k);
}
void chat_keyboard_open(ChatKeyboard *k,bool (*callback)(const char *,void *),void *user){memset(k,0,sizeof *k);k->active=true;k->row=1;k->pressed=-1;k->send=callback;k->user=user;}
void chat_keyboard_cancel(ChatKeyboard *k){memset(k,0,sizeof *k);k->pressed=-1;}
bool chat_keyboard_input(ChatKeyboard *k,unsigned down,unsigned held,u64 now){
 if(!k->active)return false;
 if(down&CHAT_CANCEL){chat_keyboard_cancel(k);return true;}
 if(down&CHAT_SEND){send(k);return true;}
 if(down&CHAT_UP)k->row=k->row?k->row-1:4;
 if(down&CHAT_DOWN)k->row=k->row<4?k->row+1:0;
 unsigned columns=k->row==4?5:10;if(k->column>=columns)k->column=columns-1;
 if(down&CHAT_LEFT)k->column=k->column?k->column-1:columns-1;
 if(down&CHAT_RIGHT)k->column=(k->column+1)%columns;
 if(down&CHAT_ACCEPT)activate(k,k->row<4?(int)(k->row*10+k->column):(int)(40+k->column));
 if(down&CHAT_SHIFT)shift(k);
 if(down&CHAT_SPACE)append(k," ");
 if(down&CHAT_PAGE)k->symbols=!k->symbols;
 if(down&CHAT_DELETE){if(!k->text[0]){chat_keyboard_cancel(k);return true;}backspace(k);k->repeat=now+450;}
 else if((held&CHAT_DELETE)&&k->repeat&&now>=k->repeat){backspace(k);k->repeat=now+70;}
 if(!(held&CHAT_DELETE))k->repeat=0;
 return true;
}
static int hit(int x,int y){
 if(x<0||x>=320||y<0||y>=240)return -1;
 if(x>=288&&y>=2&&y<25)return 45;
 if(y>=66&&y<186){unsigned row=(unsigned)(y-66)/30;if((y-66)%30<28&&x%32<30)return (int)(row*10+(unsigned)x/32);return -1;}
 if(y>=190&&y<220)for(unsigned i=0;i<5;i++)if(x>=actionX[i]&&x<actionX[i]+actionW[i])return (int)i+40;
 return -1;
}
void chat_keyboard_touch(ChatKeyboard *k,int x,int y,bool pressed,bool released){
 if(!k->active)return;
 int key=hit(x,y);
 if(pressed)k->pressed=key;
 if(released){if(key>=0&&key==k->pressed){if(key<40){k->row=(unsigned)key/10;k->column=(unsigned)key%10;}else if(key<45){k->row=4;k->column=(unsigned)key-40;}activate(k,key);}k->pressed=-1;}
}
static void draw_key(const ChatKeyboard *k,const ChatKeyboardWidgets *w,int key,float x,float y,float width,const char *label,bool selected){
 w->box(x+1,y+1,width-2,27,selected?0xffd23f:0xffffff);w->center(x+width/2,y+7,width<48?11:10,0x063a6e,label);
 if(k->pressed==key)w->box(x+3,y+26,width-6,2,0x063a6e);
}
void chat_keyboard_draw(const ChatKeyboard *k,u64 now,const ChatKeyboardWidgets *w){
 if(!k->active)return;
 w->box(0,0,320,240,0x07589a);w->text(8,5,12,0xffffff,"Say something");char count[24];snprintf(count,sizeof count,"%u/%u",characters(k->text),CHAT_KEYBOARD_CHARS);w->text(225,7,9,0xffffff,count);w->box(290,3,26,22,0xffd23f);w->center(303,7,11,0x063a6e,"X");
 w->box(5,29,310,32,0xffffff);
 char lines[8][CHAT_KEYBOARD_BYTES]={{0}};unsigned line=0;size_t used=0;
 for(const unsigned char *p=(const unsigned char *)k->text;*p;){size_t n=1;while(p[n]&&(p[n]&0xc0)==0x80)n++;char trial[CHAT_KEYBOARD_BYTES];memcpy(trial,lines[line],used);memcpy(trial+used,p,n);trial[used+n]=0;
  if(used&&w->width(11,trial)>290&&line<7){line++;used=0;}
  memcpy(lines[line]+used,p,n);used+=n;lines[line][used]=0;p+=n;
 }
 unsigned first=line?line-1:0;for(unsigned i=first;i<=line;i++)w->text(11,31+(i-first)*13,11,0x063a6e,lines[i]);
 if(!k->text[0])w->text(11,38,10,0x063a6e,"Touch keys or use the D-Pad and A");
 if(k->text[0]&&(now/500)%2)w->box(12+w->width(11,lines[line]),31+(line-first)*13,1,12,0x063a6e);
 for(unsigned row=0;row<4;row++)for(unsigned col=0;col<10;col++){char out[2];draw_key(k,w,(int)(row*10+col),col*32,66+row*30,32,letter(k,row,col,out),k->row==row&&k->column==col);}
 const char *actions[]={k->caps?"CAPS":"Shift",k->symbols?"abc":"áé#","Space","Del","Send"};
 for(unsigned i=0;i<5;i++)draw_key(k,w,40+(int)i,actionX[i],190,actionW[i],actions[i],(k->row==4&&k->column==i)||(i==0&&k->shift)||(i==1&&k->symbols));
 w->text(7,227,9,0xffffff,k->error[0]?k->error:"START: send   SELECT: close   X: shift   Y: space");
}
