#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <qdos.h>

#include "image.h"

#define DEBUG(x) { Fill(0,8,0); printAt(&font,0,0,x); showScratch(); }
//#undef DEBUG

#undef PROFILE
#define FPS

#ifdef PROFILE
#undef FPS
#endif

#define IMMORTAL

library lib,font;	// Image libraries

#define SPRITESX 11			// Number of invaders in a row
#define SPRITESY 5			// Number of invader rows
#define SPRITES (SPRITESX*SPRITESY)	// Total number of invaders
#define MAXBULLETS 4

#define WIDTH 224
#define XMIN ((256-WIDTH)/2)
#define XMAX (XMIN+WIDTH)

char *drive="";

struct player
{
	sprite sprites[SPRITES];
	unsigned int newDelta,invaderCount,shotCount,score,lives,wave;
	int direction;
};

struct player players[2];

unsigned int credits=0,currentPlayer=0,gameMode=0;

sprite bullets[3];
unsigned char bulletTypes[3];	// Invader sprites
unsigned maxBulletCount=1,bulletCount=0,shotCount=0;

sprite player,ufo,player_bullet;	// Other sprites
unsigned int playerVisible;

unsigned int highScore=0;			// Player scores
int invaderScores[5]={30,20,20,10,10};
unsigned char ufoScores[]={10,05,05,10,15,10,10,05,30,10,10,10,05,15,10,05};
unsigned char ufoScorePointer=0;

unsigned int invaderSoundTimer;

#ifdef FPS
unsigned int fpsCounter,fpsTimer;
#endif

unsigned int xPrint(unsigned int chars)
{
	return XMIN+((XMAX-XMIN)-6*chars)/2;
}

unsigned int divu10(unsigned int n)
{
    unsigned q, r;

    q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q >> 3;
    r = n - (((q << 2) + q) << 1);

    return q + (r > 9);
}

unsigned int mul10(unsigned int z)
{
        return (z<<3)+(z<<1);
}

void scorePrint(char *c,unsigned int z)
{
        int a;
        unsigned int zz;
        
        for(a=3;a>=0;a--)
        {
                zz=divu10(z);
                c[a]='0'+(z-mul10(zz));
                z=zz;
        }
}

void printScores()
{
	char s[80],*sp=s;
	char p1[5],hs[5],p2[5];

	p1[4]=hs[4]=p2[4]='\0';

	scorePrint(p1,players[0].score);
	scorePrint(hs,highScore);
	scorePrint(p2,players[1].score);

	bgFill(48,48+8,0);
	sprintf(s,"%4s     %4s     %4s",p1,hs,p2);
	printAtBG(&font,xPrint(strlen(s)),16+32,s);	
}

/////////////////////   
// TIME AND FRAMES //
/////////////////////                   

unsigned int frameCounter=0,lastFrame;  

unsigned int getFrames()
{
        unsigned int frames=*FRAMES;

        if(frames<lastFrame) frameCounter+=0x10000;
        lastFrame=frames;               

        return  frameCounter+frames;
}

void msleep(unsigned int frames)
{
	unsigned int end=getFrames()+frames;

	while(getFrames()<end);
}

int keysleep(unsigned int frames)
{
	unsigned int target=getFrames()+frames;

	while(getFrames()<target)
	{
		if(keyrow(2)&8) // Coin key 'C' ?
		{
			credits++; 
			while(keyrow(2)&8) ;

			return 0;
		}

		if((keyrow(4)&8)&&(credits>0)) // Player 1 start '1' ?
		{
			gameMode=1;	
			return 1; // Player 1 start?
		}

		if((keyrow(6)&2)&&(credits>1)) // Player 2 start '2' ?
		{
			gameMode=2;
			return 2;  // Player 2 start?
		}
	}

	return 0;
}

int slowPrintAt(unsigned int x,unsigned y,char *s)
{
	unsigned key;

	while(*s!=0)
	{
		unsigned int frames=getFrames();

		// Coin key 'C' ?

		if(keyrow(2)&8)
		{
			credits++; 
			while(keyrow(2)&8) ;

			return 1;
		}

		if((keyrow(4)&8)&&(credits>0))
		{
			gameMode=1;	
			return 1; // Player 1 start?
		}

		if((keyrow(6)&2)&&(credits>1))
		{
			gameMode=2;
			return 2;  // Player 2 start?
		}

		if(*s!=32) printCharAt(&font,x,y,*s);

                showScratch();

		s++;
		x+=6;

		while(frames+5>getFrames());
	}

	return 0;
}

///////////////////////////////
// Handle keyboard commands. //
///////////////////////////////

timer keyTimer;

void handleKeys(unsigned int frames)
{
	unsigned int key;

	if(!playerVisible)
	{
		if(player.timer.value>frames) return;

		playerVisible=1;
		player.timer.value=frames;
		keyTimer.value=frames;

		return;
	}

	while(frames>=keyTimer.value)
	{
		key=keyrow(1);	// Read the bottom row of the keyboard

		keyTimer.value+=keyTimer.delta;

        	if(key)		// If a key was pressed
		{
        	        if((key&2)&&(player.x>XMIN))
				player.x--;	// Move left
                	else if((key&16)&&(player.x<XMAX-player.image[0]->x*4))
				player.x++;	// Move right

			if((key&64)&&(player_bullet.y<0))	// Fire (if not already fired)
                	{
	                        player_bullet.y=player.y-8;		// Set player bullets start location
        	                player_bullet.x=player.x+3;
				player_bullet.timer.value=frames;
				shotCount++;
			}
        	}
	}

	if(keyrow(4)&32)
	{
		while(keyrow(4)&32); 
		while((keyrow(4)&32)==0); 
		while(keyrow(4)&32); 
	}
}

//////////////////////////
// HandleInvaderBullets //
//////////////////////////

unsigned char reload;

int handleInvaderBullets(unsigned int frames)
{
	unsigned int i;

	if(bulletCount>0) for(i=0;i<maxBulletCount;i++)
	{
	        if((bullets[i].y>-1)&&(bullets[i].timer.value<=frames))	// Fired?
	        {
			bullets[i].timer.value=frames+bullets[i].timer.delta;
	       	        bullets[i].y+=(players[currentPlayer].invaderCount<=8?5:4);
			bullets[i].currentImage=(bullets[i].currentImage+1)&3;

			if(bullets[i].y>=player.y)	// Reached the bottom
	              	{
				bullets[i].y=-1;
				bulletCount--;

				if(players[currentPlayer].score<200) reload=frames+0x30;
				else if(players[currentPlayer].score<1000) reload=frames+0x10;
				else if(players[currentPlayer].score<2000) reload=frames+0x0B;
				else if(players[currentPlayer].score<3000) reload=frames+0x08;
				else reload=getFrames()+0x07;
			
				#ifndef IMMORTAL	 
				if((bullets[i].x>=player.x)&&(bullets[i].x<player.x+16))
				{
					unsigned int j;

					for(j=0;j<10;j++)
					{			
						player.currentImage=3;
						spritePlot(&player);
						showScratch();
						msleep(10);

						player.draw=0;
						spritePlot(&player);
						player.draw=1;

						player.currentImage=2;
						spritePlot(&player);
						showScratch();
						msleep(10);
						player.draw=0;
						spritePlot(&player);
						player.draw=1;
					}

					player.currentImage=0;
					return 1;
				}
				#endif
	       		}
			else
			{
				if(peek(bullets[i].y+4,bullets[i].x)
				 ||peek(bullets[i].y+6,bullets[i].x))
				{
					// Hit something!

					bullets[i].currentImage=4;
					bullets[i].mask=1;
					bullets[i].x-=2;
					bgSpritePlot(&bullets[i]);
	
					bullets[i].currentImage=0;
					bullets[i].mask=0;
					bullets[i].y=-1;
					bulletCount--;
				}
			}
		}

		if(bullets[i].y>-1) spritePlot(&bullets[i]);	// Draw bullet if still active
	}

	return 0;
}

////////////////////////////////
// HandlePlayerBullet         //
//                            //
// returns: 1 - wave complete //
////////////////////////////////

int handlePlayerBullet(unsigned int frames)
{
	unsigned int i,skipped;

        while((player_bullet.y>-1)&&(player_bullet.timer.value<frames))
        {
                for(skipped=frames-player_bullet.timer.value+1;skipped--;skipped>=0)
                {
	       	        player_bullet.y-=2;

			player_bullet.timer.value=frames+player_bullet.timer.delta;

			if(player_bullet.y<=32)	// Reached the top
       		       	{
				// Explosion!!!
		
				player_bullet.currentImage++;
				spritePlot(&player_bullet);
       		              	player_bullet.currentImage--;
	
				player_bullet.y=-1;
				return 0;
			}

			if(peek(player_bullet.y,player_bullet.x+2))
			{
				// Invader hit?

				for(i=0;i<SPRITES;i++)
				{
					sprite *s=&players[currentPlayer].sprites[i];

					if((s->y>-1)&&(s->x-3<player_bullet.x)
					&&(s->x+9>player_bullet.x)
					&&(s->y<=player_bullet.y)
					&&(s->y+8>=player_bullet.y))
					{
						players[currentPlayer].newDelta=(50*players[currentPlayer].invaderCount)/SPRITES;

						if(players[currentPlayer].newDelta<s->timer.delta)
						{
							unsigned int j;
	
							for(j=0;j<SPRITES;j++) players[currentPlayer].sprites[j].timer.delta=players[currentPlayer].newDelta;
						}

						// Set up explosion at the invader's locations

						player_bullet.currentImage=2;
						player_bullet.x=players[currentPlayer].sprites[i].x;
						player_bullet.y=players[currentPlayer].sprites[i].y;

						spritePlot(&player_bullet);

	              				player_bullet.currentImage=0;

						s->draw=0;
        	                	        bgSpritePlot(s);
     		           	                s->draw=1;
	
						s->y=-1;	

						player_bullet.y=-1;

						players[currentPlayer].score+=invaderScores[i/SPRITESX];
						printScores();
			
						return --players[currentPlayer].invaderCount==0;	// Can only hit one thing!
					}
				}

				if((ufo.y>-1)&&(ufo.x<player_bullet.x)
					     &&(ufo.x+9>player_bullet.x)
				             &&(ufo.y<=player_bullet.y)
				             &&(ufo.y+8>=player_bullet.y))
				{

					player_bullet.currentImage=3;
					player_bullet.x=ufo.x;
					player_bullet.y=ufo.y;
					spritePlot(&player_bullet);
					player_bullet.currentImage=0;

					player_bullet.y=-1;
					ufo.x=-1;

					players[currentPlayer].score+=ufoScores[ufoScorePointer]*10;
					printScores();

					ufoScorePointer=(ufoScorePointer+1)&15;

					return 0;
				}

                                // Base?

                                player_bullet.currentImage=4;
                                player_bullet.y-=5;
				player_bullet.mask=1;
                                bgSpritePlot(&player_bullet);
				player_bullet.mask=0;
                                player_bullet.currentImage=0;

                                player_bullet.y=-1;
			
                                return 0;
			}
		}
	}

	if(player_bullet.y>-1) spritePlot(&player_bullet);	// Draw bullet if still active

	return 0;	// Wave still ongoing
}

/////////////////
// invaderFire //
/////////////////

void invaderFire(unsigned int frames)
{
	//ShotReloadRate:
	//; The tables at 1CB8 and 1AA1 control how fast shots are created. The speed is based
	//; on the upper byte of the player's score. For a score of less than or equal 0200 then
	//; the fire speed is 30. For a score less than or equal 1000 the shot speed is 10. Less
	//; than or equal 2000 the speed is 0B. Less than or equal 3000 is 08. And anything
	//; above 3000 is 07.
	//;
	//; 1CB8: 02 10 20 30

	if((bulletCount<maxBulletCount)&&(reload<getFrames()))
	{
		unsigned int i,j;

		for(j=0;j<maxBulletCount;j++)
		{
			if(bullets[j].y==-1)
			{
				int k;

				// Pick type

				bulletTypes[j]=rand()/(RAND_MAX/3);

				switch(bulletTypes[j])
				{
					case 0:  bullets[j].image[0]=&lib.images[19];
						 bullets[j].image[1]=&lib.images[20];
						 bullets[j].image[2]=&lib.images[21];
						 bullets[j].image[3]=&lib.images[22];
						 break;
					case 1:  bullets[j].image[0]=&lib.images[23];
						 bullets[j].image[1]=&lib.images[24];
						 bullets[j].image[2]=&lib.images[23];
						 bullets[j].image[3]=&lib.images[25];
						 break;
					default: bullets[j].image[0]=&lib.images[16];
						 bullets[j].image[1]=&lib.images[9];
						 bullets[j].image[2]=&lib.images[17];
						 bullets[j].image[3]=&lib.images[13];
						 break;
				}

				bulletCount++;

				// Bullet type 0 fires either at the player, or the closest to the player
				if(bulletTypes[j]==0)
				{
					int nearest=INT_MAX;

					for(k=SPRITES-1;k>=0;k--)
					{
						if(players[currentPlayer].sprites[k].y>-1)
						{
							int d=players[currentPlayer].sprites[k].x>player.x?players[currentPlayer].sprites[k].x-player.x
										   :player.x-players[currentPlayer].sprites[k].x;

							if(d<nearest)
							{
								i=k; nearest=d;
							}
						}
					}
				}
				else 
				{
					unsigned int k;

					// Make bullet come from random bottom invader

					do
					{
						i=rand()/(RAND_MAX/SPRITES);
					}
					while(players[currentPlayer].sprites[i].y==-1);

					for(k=i+11;k<SPRITES;k+=11) if(players[currentPlayer].sprites[k].y>-1) i=k;

				}

				bullets[j].y=players[currentPlayer].sprites[i].y+8;
				bullets[j].x=players[currentPlayer].sprites[i].x+4;
				bullets[j].timer.value=frames;
				bullets[j].timer.delta=3;
				bullets[j].currentImage=0;

				break;
			}
		}
	}
}

///////////////////////////////
// handleInvaders            //
//                           //
// returns: 1 - invaders win //
//          0 - still going! // 
///////////////////////////////

int bounce=0;

int handleInvaders(unsigned int frames)
{
	unsigned int i,bounce=0;

        for(i=0;i<SPRITES;i++)
        {
		sprite *s=&players[currentPlayer].sprites[i];

		if((s->y>-1)&&(s->timer.value<=frames))	// Time to move?
		{
			unsigned int newX=s->x
				+((players[currentPlayer].direction==1)?s->dx:-s->dx);

			if(!bounce)
			{
				if(((players[currentPlayer].direction==-1)&&(newX<=XMIN))
				 ||((players[currentPlayer].direction==1)&&(newX>=XMAX-17)))
				{
					bounce=1;

					break;
				}
			}


			// Clear old invader from BG
			s->draw=0; bgSpritePlot(s); s->draw=1;

			s->x=newX;				// Move invader
		        s->currentImage=1-s->currentImage; 	// Animate
			s->timer.value+=s->timer.delta;		// Set up timer for next movement 
			bgSpritePlot(s);	// Draw invader on BG

		}
        }

/*
	if(frames>=invaderSoundTimer)
	{
		invaderSoundTimer+=players[currentPlayer].newDelta;
		//do_sound(2000,200,200,5,1,0,0,0);
	}
*/

	if(bounce)	// Move the invaders down and reverse direction
	{
		unsigned int lowest=0;

		bounce=0;

		players[currentPlayer].direction=-players[currentPlayer].direction;

                for(i=0;i<SPRITES;i++)
                {
                        if(players[currentPlayer].sprites[i].y>-1)
                        {
                                players[currentPlayer].sprites[i].draw=0;
                                bgSpritePlot(&players[currentPlayer].sprites[i]);
                                players[currentPlayer].sprites[i].draw=1;

                                lowest=max(lowest,players[currentPlayer].sprites[i].y);
                        }
                }

		if(lowest+16>=195) bgFill(lowest+8,lowest+16,0);

                for(i=0;i<SPRITES;i++)
                {
                        sprite *s=&players[currentPlayer].sprites[i];

                        if(s->y>-1)     // Is sprite alive?
                        {
                                s->y+=8;        // Move invader down

                                // Game over?
                                if(s->y>=player.y)  return 1;

                                bgSpritePlot(s);
                        }
                }
	}

	invaderFire(frames);

	return 0;
}

///////////////
// handleUFO //
///////////////

void handleUFO(unsigned int frames)
{
	if(ufo.x==-1)	// No ufo moving?
        {
		// TODO: Convert to timer?

		unsigned int r=rand(); // Get a random integer

                if((r&255)==0)		// Every 128 frames fire a UFO
                {
                        ufo.x=(r&256)?XMIN:(XMAX-1-ufo.image[0]->x);	// Use a random bit for side to start on
			ufo.dx=(ufo.x==XMIN)?2:-2;		//  direction depends on start location
			ufo.timer.value=0;
			ufo.timer.delta=1;		// Move every 5 frames
                }
        }
        else
        {
		if(ufo.timer.value<frames) // Is it time to move?
		{
			ufo.x+=ufo.dx;		// Move

       	        	if((ufo.x<=XMIN)||(ufo.x>=(XMAX-ufo.image[0]->x)))	// Reached other end?
			{
				ufo.x=-1;	// Switch off the UFO
				return;
			}

			ufo.timer.value=frames+ufo.timer.delta;	// Set the next time to run
		}

                spritePlot(&ufo);	// Draw the UFO
       }
}

/////////////
// setupBG //
/////////////

void setupBG(unsigned int bases,unsigned int line)
{
        unsigned int i;
        char buffer[80];
        sprite base;

        clsAll();

	sprintf(buffer,"%d",players[currentPlayer].lives);
	printAt(&font,xPrint(strlen(buffer)),255-8,buffer);

        printAt(&font,xPrint(26),32,"SCORE<1> HI-SCORE SCORE<2>");
	sprintf(buffer,"CREDIT %02d",credits);
        printAt(&font,XMAX-6*strlen(buffer),255-8,buffer);

        if(bases) for(i=0;i<4;i++)
        {
                sprite s;

		s.mask=0; s.draw=1;
                s.image[0]=&lib.images[26];
                s.currentImage=0;
                s.x=XMIN+i*48+32; s.y=195;

                spritePlot(&s);
        }

        if(line)
        {
                // Bottom green line

                base.image[0]=&lib.images[28];
                base.currentImage=0;
                base.mask=0;
                base.draw=1;
                base.y=245;

	        for(base.x=XMIN;base.x<XMAX;base.x+=32) spritePlot(&base);
        }

        base.image[0]=&lib.images[8];
        base.currentImage=0;
        base.y=255-8;
        base.x=XMIN+24;

        for(i=0;i<players[currentPlayer].lives-1;i++)
        {
                spritePlot(&base);
                base.x+=16;
        }


        base.image[0]=&lib.images[28];
        base.y--;

        for(i=0;i<256;i+=32) // WHAT DOES THIS DO?
        {
                base.x=i;
                //spritePlot(&base);
        }

        showScratch();
        initBG();
}

///////////////////
// setupInvaders //
///////////////////

void setupInvaders(unsigned int frames)
{
	unsigned int i;

	for(i=0;i<SPRITES;i++)
        {
		sprite *s=&players[currentPlayer].sprites[i];

		int x=(i%11),y=i/11+players[currentPlayer].wave*16,ss=y==0?2:(y<3?4:0);

                s->image[0]=&lib.images[ss];
                s->image[1]=&lib.images[ss+1];
                s->currentImage=0;

                s->x=45+x*16+(y==0?2:0)+1;
                s->y=y*16+80;

                s->dx=1;
                s->dy=0;

                s->timer.value=frames+(SPRITES-i);
                s->timer.delta=50;

		s->mask=0;
		s->draw=1;

		bgSpritePlot(s);
        }

	invaderSoundTimer=0;

	players[currentPlayer].invaderCount=SPRITES;
	players[currentPlayer].newDelta=50;
}

//////////////////
// IntroScreens //
//////////////////

void startGameScreen()
{
	while(gameMode==0)
	{
		setupBG(0,0);
		BGtoScratch();
		printScores();

		printAt(&font,xPrint(4),90,"PUSH");

		if(credits>1) printAt(&font,xPrint(20),130,"1 OR 2PLAYERS BUTTON");
		else printAt(&font,xPrint(19),130,"ONLY 1PLAYER BUTTON");

		showScratch();

		keysleep(INT_MAX);
	}

	// gameMode will be 1 or 2 here
}

void introScreens()
{
	unsigned int x=xPrint(0)-56; // Middle of screen

	clsAll();
	setupBG(0,0);

	while(1)
	{
		BGtoScratch(); printScores(); showScratch();

		if(slowPrintAt(xPrint(4),70,"PLAY")) return;
		if(slowPrintAt(xPrint(14),100,"SPACE INVADERS")) return;

		if(keysleep(50)) return;

		printAt(&font,xPrint(21),130,"*SCORE ADVANCE TABLE*");
		ufo.x=x-3; ufo.y=150;
		ufo.image[0]=&lib.images[29];
		spritePlot(&ufo); ufo.image[0]=&lib.images[7]; ufo.y=-1;

		players[0].sprites[0].x=x; players[0].sprites[0].y=170; spritePlot(&players[0].sprites[0]);
		players[0].sprites[SPRITES/2].x=x-1; players[0].sprites[SPRITES/2].y=190; spritePlot(&players[0].sprites[SPRITES/2]);
		players[0].sprites[SPRITES-1].x=x-1; players[0].sprites[SPRITES-1].y=210; players[0].sprites[SPRITES-1].image[0]=&lib.images[30]; spritePlot(&players[0].sprites[SPRITES-1]); players[0].sprites[SPRITES-1].image[0]=&lib.images[0];

		showScratch();

		if(slowPrintAt(x+16,150,"=? MYSTERY")) return; 
		if(slowPrintAt(x+16,170,"=30 POINTS")) return; 
		if(slowPrintAt(x+16,190,"=20 POINTS")) return; 
		if(slowPrintAt(x+16,210,"=10 POINTS")) return; 

		if(keysleep(50)) return;

		// Score table

		BGtoScratch();

		printScores();

		printAt(&font,xPrint(12),100,"INSERT  COIN"); showScratch();
		if(slowPrintAt(xPrint(16),140,"<1 OR 2 PLAYERS>")) return;
		if(slowPrintAt(xPrint(17),140+24,"*1 PLAYER  1 COIN")) return;
		if(slowPrintAt(xPrint(18),140+48,"*2 PLAYERS 2 COINS")) return;

		if(keysleep(100)) return;
	}
}

void initiate(unsigned int convert)
{
	char name[80];

	init();

	// sprites

	if(convert)
	{
		sprintf(name,"%sinvaders_lib",drive);
		loadLibrary(&lib,name,1);
		sprintf(name,"%sinvaders_blb",drive);
		bSaveLibrary(&lib,name);
	}
	else
	{
		sprintf(name,"%sinvaders_blb",drive);
		bLoadLibrary(&lib,name,1);
	}

	// font

	if(convert)
	{
		sprintf(name,"%sfont_lib",drive);
		loadLibrary(&font,name,1); 

		sprintf(name,"%sfont_blb",drive);
		bSaveLibrary(&font,name);

		puts("Sprite convertion completed.");

		exit(0);
	}
	else
	{
		sprintf(name,"%sfont_blb",drive);
		bLoadLibrary(&font,name,1); 
	}

 	if(lib.n==0)
 	{
 		puts("Error: Loaded 0 images from font library!\n");
 		exit(1);
 	}
}

//////////
// game //
//////////

int gameLoop()
{
	unsigned int frames,lastFrames=0;

	#ifdef PROFILE
	unsigned int profileCounts[10],profileCounter=0,profileN;
	#endif

	#ifdef FPS
	char s[80];
	fpsCounter=0;
	fpsTimer=getFrames()+50;
	#endif
	
	player.x=XMIN;

	#ifdef PROFILE
	for(frames=0;frames<10;frames++) profileCounts[frames]=0;
	#endif

	printScores();

	while(1)
	{
		// Wait for next frame

		#ifdef PROFILE
		profileN=0;

		if(profileCounter==0) profileCounter=getFrames();

		// 0
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		
		#endif

		do
		{
			frames=getFrames();
		}
		while(frames==lastFrames);

		lastFrames=frames;

		#ifdef PROFILE
		// 1
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		// Move the invaders on the BG screen

		if(handleInvaders(frames)) return 2; // GAME OVER!

		#ifdef PROFILE
		// 2
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		BGtoScratch();	// Copy the BG to scratch...

		#ifdef PROFILE
		// 3
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		handleKeys(frames);	// Fire/move the player

		// 4
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		if(handlePlayerBullet(frames)) return 0; // LEVEL CLEARED!

		// 5
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		handleUFO(frames);

		// 6
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		if(handleInvaderBullets(frames))
		{
	        	showScratch();
			return 1; // LOSE A LIFE!
		}

	        spritePlot(&player);

		// 7
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		#ifdef FPS
		fpsCounter++;
		if(fpsTimer<getFrames())
		{
			sprintf(s,"%d",fpsCounter);

			fpsTimer=getFrames()+50;
			fpsCounter=0;
		}
		printAt(&font,0,0,s);
		#endif

		// 8
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		showScratch();

		// 9
		#ifdef PROFILE
		profileCounts[profileN++]+=(getFrames()-profileCounter);
		profileCounter=getFrames();
		#endif

		#ifdef PROFILE
		if(keyrow(3))
		{
			for(frames=0;frames<10;frames++) printf("%d\t%d\n",frames,profileCounts[frames]);
			exit(0);
		}
		#endif
	}

}

///////////////
// setupGame //
///////////////

void setupGame(unsigned int frames)
{
	unsigned int i;

	setupInvaders(frames);

	for(i=0;i<MAXBULLETS;i++)
	{
		bullets[i].x=0; bullets[i].y=-1;
		bullets[i].dx=0; bullets[i].dy=0;

		bullets[i].image[0]=&lib.images[16];
		bullets[i].image[1]=&lib.images[9];
		bullets[i].image[2]=&lib.images[17];
		bullets[i].image[3]=&lib.images[13];
		bullets[i].image[4]=&lib.images[18];

		bullets[i].currentImage=0;
		bullets[i].mask=0;
		bullets[i].draw=1;
	}

	players[0].score=players[1].score=0;
	players[0].lives=players[1].lives=6;
	players[0].wave=players[1].wave=0;
	players[0].direction=players[1].direction=1;

	reload=0x30+getFrames();

	player.image[0]=&lib.images[8];
	player.image[1]=&lib.images[10];
	player.image[2]=&lib.images[14];
	player.image[3]=&lib.images[31];
	player.currentImage=0;
	player.mask=0;
	player.draw=1;
	player.x=0;
	player.y=256-player.image[0]->y-24;

	player_bullet.image[0]=&lib.images[12];
	player_bullet.image[1]=&lib.images[15];
	player_bullet.image[2]=&lib.images[11];
	player_bullet.image[3]=&lib.images[27];
        player_bullet.image[4]=&lib.images[32];
	player_bullet.currentImage=0;
	player_bullet.x=0;
	player_bullet.y=-1;
	player_bullet.timer.value=0;
	player_bullet.timer.delta=1;
	player_bullet.mask=0;
	player_bullet.draw=1;
	shotCount=0;

	ufo.image[0]=&lib.images[7];
	ufo.currentImage=0;
	ufo.mask=0;
	ufo.draw=1;
	ufo.x=-1;
	ufo.y=64;

	keyTimer.value=0;
	keyTimer.delta=1;
}


//////////////
// mainloop //
//////////////

void mainLoop(int convert)
{
	unsigned int i;

	initiate(convert);

	while(1)
	{
		setupGame(getFrames());

		if(credits==0) introScreens();

		startGameScreen();

		// Play!

		credits-=gameMode;

		setupGame(getFrames());

		players[0].score=players[1].score=0;	// Reset the scores

		while(players[currentPlayer].lives>0)
		{
			char s[80];
			unsigned int frames;

		        clsAll();

		        setupBG(0,0);   
               
			// TODO: Following not seen on screen 
		        printAt(&font,xPrint(10),100,currentPlayer==0?"PLAYER <1>":"PLAYER <2>");

			showScratch();

			// Blink the score of the next player

			frames=getFrames()+150;

			while(getFrames()<frames)
			{
				if((getFrames())&1)
		                	sprintf(s,"%04d     %04d     %04d",players[0].score,highScore,players[1].score);
		                else if(currentPlayer==0) 
					sprintf(s,"%c%c%c%c     %04d     %04d",'Z'+1,'Z'+1,'Z'+1,'Z'+1,highScore,players[1].score);
		                else 
					sprintf(s,"%04d     %04d     %c%c%c%c",players[0].score,highScore,'Z'+1,'Z'+1,'Z'+1,'Z'+1);

       				printAt(&font,xPrint(strlen(s)),16+32,s);

				showScratch();
			}

			setupBG(1,1); 

			// Start the player a few seconds into the game
			player.timer.value=frames+100;
			playerVisible=0;

			// Set invader timers

			for(i=0;i<SPRITES;i++)
			{
			        sprite *s=&players[currentPlayer].sprites[i];

				if(s->y>-1) s->timer.value=frames+(SPRITES-i);
			}

			// Play until death, then...

			switch(gameLoop())
			{
				case 0: break;		// Wave completed
				case 1: players[currentPlayer].lives--; break;	// Base hit
				case 2: players[currentPlayer].lives=0; break;	// Invaders hit the bottom
			}

			if(gameMode==2) currentPlayer=1-currentPlayer;
		}

		if(players[currentPlayer].score>highScore) highScore=players[currentPlayer].score;
	}
}

void benchmark()
{
	unsigned int s;

	init();
	loadLibrary(&lib,"/home/simon/test5.lib",1);
	initBG();

	for(s=0;s<3;s++)
	{
		const int n=10;

		unsigned long t,c=0,pass;
		unsigned int x=0,y=0;
		sprite sprite[8];

		for(c=0;c<8;c++)
		{
			sprite[c].image[0]=&lib.images[s];
			sprite[c].currentImage=0;
			sprite[c].x=x++;

			sprite[c].y=y;
			sprite[c].draw=1;

			y+=sprite[c].image[0]->y;
			if(y+sprite[c].image[0]->y>255) y=0;
		}

		for(pass=0;pass<2;pass++)
		{
			for(c=0;c<8;c++) sprite[c].mask=pass;

			t=getFrames()+n*50;
	
			while(t>getFrames())
			{
				unsigned int i,y=0,x=0;

				BGtoScratch();

				for(i=0;i<8;i++)
				{
					if(y+lib.images[s].y>=256)
					{
						y=0;
						x+=lib.images[s].x*2+16;
					}
	
					x++;
	
					spritePlot(&sprite[i]);
	
					y+=lib.images[s].y;

					c++;
				}

				showScratch();
			}

			showScratch();

			printf("%c %d x %d -> %d\n",pass==1?'M':' ',lib.images[s].x*4,lib.images[s].y,c);
			initBG();
		}
	}

	exit(0);
}

//////////
// main //
//////////

int main(int argc, char *argv[])
{
	int s,convert=0;;

	// Parse the args

	for(s=1;s<argc;s++)
	{
		if(strcmp(argv[s],"-bm")==0) benchmark();
		else if(strcmp(argv[s],"-c")==0) convert=1;
		else if(strcmp(argv[s],"-d")==0) drive=argv[++s];
	}

	mainLoop(convert);
}

