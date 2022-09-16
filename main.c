#include <stdio.h>
#include <stdint.h>
#include "pieces.txt"
#include "sfx.txt"

//Audio variables
int fifospace, buffer_index = 0;
int sound_id;
int BUF_SIZE=14448;

// Hardware register addresses
volatile int * LED = (int*) 0xFF200000;
volatile int * KEY = (int*) 0xFF200050;
volatile int * HEX0 = (int*) 0xFF200020;
volatile int * HEX4 = (int*) 0xFF200030;
volatile int * SW = (int*) 0xFF200040;
volatile int * audio_ptr = (int *) 0xFF203040; //Audio port address
volatile int * timer_status    = (int*) 0xFF202000;
volatile int * timer_control   = (int*) 0xFF202004;
volatile int * timer_startlow  = (int*) 0xFF202008;
volatile int * timer_starthigh = (int*) 0xFF20200C;
volatile int * snap_low  = (int*) 0xFF202010;
volatile int * snap_high = (int*) 0xFF202014;
volatile int * square = (int*) 0xC8000050;
volatile int * VGA = (int*) 0xC8000000;
volatile int * PS2_ctrl = (int *) 0XFF200104;  // PS/2 port address
volatile int * PS2_ptr = (int *) 0xFF200100; // PS/2 port address
int bytey = 0, clickbyte = 0, bytex = 0 ;
int xsel=4, ysel=4,validPress=0;
int xfirst,yfirst,tempo,moveFinish=0;

int a=0,b=0,temp=0,temp2=0;
int xloc=290;
int yloc=120;
int count = 0;


// 0 empty 1 pawn 2 rock 3 bishop 4 knight 5 queen 6 king
int turn = 0;
int board[8][8] = {   //keeps piece types
	{2,1,0,0,0,0,1,2},
    {4,1,0,0,0,0,1,4},
    {3,1,0,0,0,0,1,3},
    {5,1,0,0,0,0,1,5},
    {6,1,0,0,0,0,1,6},
    {3,1,0,0,0,0,1,3},
    {4,1,0,0,0,0,1,4},
    {2,1,0,0,0,0,1,2}
	
};

int blackWhite[8][8] = {   //keeps piece colors
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1},
    {2,2,0,0,0,0,1,1}
};
int moves[8][8];   //valid move array

void moveSound(){
	buffer_index=0;
	while (1){
	fifospace = *(audio_ptr + 1); // read the audio port fifospace register
		if ( ((fifospace & 0x00FF0000)>>16) > 96){ // check RARC, for >75% full
			while ( (fifospace & 0x00FF0000) || (buffer_index == BUF_SIZE) ){
			*(audio_ptr + 2) = move_sound[buffer_index]; //Leftdata
			*(audio_ptr + 3) = move_sound[buffer_index]; //Rightdata
			buffer_index+=1;
			fifospace = *(audio_ptr + 1); // read the audio port fifospace register
			}
		}
			if(buffer_index>=BUF_SIZE)
				break;
		}
}

void sevenSD(int *c){   //converts binary numbers to seven segment representation
	switch(*c){
		case 0: *c= 0x3f; break;
		case 1: *c= 0x06; break;
		case 2: *c= 0x5b; break;
		case 3: *c= 0x4f; break;
		case 4: *c= 0x66; break;
		case 5: *c= 0x6d; break;
		case 6: *c= 0x7d; break;
		case 7: *c= 0x07; break;
		case 8: *c= 0x7f; break;
		case 9: *c= 0x6f; break;

		}
}

void set_timemode(int *increment, int *time_mode){
	//Read the switches and set the time mode accordingly
	int sw_data;
	int increment_data;
	sw_data = *SW;
	sw_data &= 0x3FF;
	if((sw_data&0x380)){
		increment_data=(sw_data&0x380);
		if(increment_data==128)
			*increment = 1;
		if(increment_data==256)
			*increment = 2;
		if(increment_data==512)
			*increment = 4;			
	}
	sw_data &= 0x7F;
	if(sw_data==1)
		*time_mode=60;
	if(sw_data==2)
		*time_mode=180;
	if(sw_data==4)
		*time_mode=300;
	if(sw_data==8)
		*time_mode=600;
	if(sw_data==16)
		*time_mode=900;
	if(sw_data==32)
		*time_mode=1800;
	if(sw_data==64)
		*time_mode=3600;
}

void set_A9_IRQ_stack(void){
int stack, mode;
stack = 0xFFFFFFF8; // top of A9 on-chip memory, aligned to 8 bytes
/* change processor to IRQ mode with interrupts disabled */
mode = 0b11010010;
asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
/* set banked stack pointer */
asm("mov sp, %[ps]" : : [ps] "r" (stack));
/* go back to SVC mode before executing subroutine return! */
mode = 0b11010011;
asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
}

void config_interrupt (int N, int CPU_target){
int reg_offset, index, value, address;
/* Configure the Interrupt Set-Enable Registers (ICDISERn).
* reg_offset = (integer_div(N / 32) * 4
* value = 1 << (N mod 32) */
reg_offset = (N >> 3) & 0xFFFFFFFC;
index = N & 0x1F;
value = 0x1 << index;
address = 0xFFFED100 + reg_offset;
/* Now that we know the register address and value, set the appropriate bit */
*(int *)address |= value;
/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
* reg_offset = integer_div(N / 4) * 4
* index = N mod 4 */
reg_offset = (N & 0xFFFFFFFC);
index = N & 0x3;
address = 0xFFFED800 + reg_offset + index;
/* Now that we know the register address and value, write to (only) the appropriate byte */
*(char *)address = (char) CPU_target;
}

void disable_A9_interrupts(void){
int status = 0b11010011;
asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

void config_GIC(void){
config_interrupt (79, 1); // configure the KEYs parallel port (Interrupt ID = 79)
// Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all priorities
*((int *) 0xFFFEC104) = 0xFFFF;
// Set CPU Interface Control Register (ICCICR). Enable signaling of interrupts
*((int *) 0xFFFEC100) = 1;
// Configure the Distributor Control Register (ICDDCR) to send pending interrupts to CPUs
*((int *) 0xFFFED000) = 1;
}

void enable_A9_interrupts(void){
int status = 0b01010011;
asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

int isDifferent(){   //checks if cursor clicked a new square
	
	if((xfirst!=xsel) || (yfirst!=ysel)) 
		return 1;
	else
		return 0;
	
}

int swapcolor(int color){   //changes the background color of the board
	int result=0;
	if(color == 0xB326B326)
		result = 0xF695F695;
	if(color == 0xF695F695)
		result = 0xB326B326;
	return result;
}

int boardcolor(int x, int y){   //color selection for printing empty squares
	int color;
	if((x+y)%2)
		color=0xB326B326;
	else
		color=0xF695F695;
	return color;
}

int getcolor(int y, int x, int id){   //gets the pixel data of each piece
	int color;
	if(id == 1)
		color = pawn[y][2*x];
	if(id == 2)
		color = rook[y][2*x];
	if(id == 3)
		color = bishop[y][2*x];
	if(id == 4)
		color = knight[y][2*x];
	if(id == 5)
		color = queen[y][2*x];
	if(id == 6)
		color = king[y][2*x];
	return color^1;
}

void printsquare(int sqx, int sqy, int turn, int id){   //prints pieces on the board
	int val=0;
	int x=0;
	int y=0;
	int offset = ((15*sqx)+(0x1E00*sqy));		//The offset value to access each individual square
	int color;
	if(turn)
		color=0;
	if(turn==0)
		color=0xF79EF79E;
	if(id==0)
		color=boardcolor(sqx,sqy);
	if(id==8)									//"Check" color. Not used
		color = 0xF800F800;
	if(id==9)									//"Highlight" color. Not used
		color = 0x34C634C6;
	
	while (y<30){								//Print the whole square
		val=(y<<8)+(x)+(offset);
		if(id==9){								//Print the highlight. Not used
			if(((y==0)||(y==29))){
				if(((x<4)||(x>11)))
					*(square+val)=color;
			}
			if(((x==0)||(x==14))&&(((y<8)||(y>22))))
				*(square+val)=color;	
		}
		else{	
			if(getcolor(y, x, id))				//Get the correct color and print
				*(square+val)=color;
		}
		x+=1;
			if(x==15){
				x=0;
				y+=1;
			}
	}
}

void drawPieces(){   //prints all pieces on the board
	int x,y;
	 for(x = 0; x < 8; x++){
        for(y = 0; y < 8; y++){

			printsquare(x,y,blackWhite[x][y]-1,board[x][y]);
 
        }
    }	
}

int LegalMove(){   //checks if it is a valid move or not
	int id;
	int i,j;
	int legal=0;
	int pcol;
	
	for(i=0; i<8; i++){
		for(j=0; j<8; j++){
			moves[i][j] = 0;   //clears valid move array
		}
	}
	
	id = board[xfirst][yfirst];
	pcol = blackWhite[xfirst][yfirst];
	
	if(id==1){   //valid pawn moves
			if(board[xfirst][yfirst-1]==0){
				moves[xfirst][yfirst-1]=1;
				if(board[xfirst][yfirst-2]==0){
					if(yfirst==6)
						moves[xfirst][yfirst-2]=1;
				}
			}
			if(blackWhite[xfirst+1][yfirst-1])
				moves[xfirst+1][yfirst-1]=1;
			if(blackWhite[xfirst-1][yfirst-1])
				moves[xfirst-1][yfirst-1]=1;			
	}
	if(id==2){   //valid rook moves
		for(i=1;i<=xfirst;i++){
			if(board[xfirst-i][yfirst]==0)
				moves[xfirst-i][yfirst]=1;
			else{
				if(blackWhite[xfirst-i][yfirst]==pcol)
					break;
				moves[xfirst-i][yfirst]=1;
				break;
			}
		}	
		for(i=1;i<=(7-xfirst);i++){
			if(board[xfirst+i][yfirst]==0)
				moves[xfirst+i][yfirst]=1;
			else{
				if(blackWhite[xfirst+i][yfirst]==pcol)
					break;
				moves[xfirst+i][yfirst]=1;
				break;
			}
		}
		for(i=1;i<=yfirst;i++){
			if(board[xfirst][yfirst-i]==0)
				moves[xfirst][yfirst-i]=1;
			else{
				if(blackWhite[xfirst][yfirst-i]==pcol)
					break;
				moves[xfirst][yfirst-i]=1;
				break;
			}
		}
		for(i=1;i<=(7-yfirst);i++){
			if(board[xfirst][yfirst+i]==0)
				moves[xfirst][yfirst+i]=1;
			else{
				if(blackWhite[xfirst][yfirst+i]==pcol)
					break;
				moves[xfirst][yfirst+i]=1;
				break;
			}
		}	
	}
	
	if(id==3){  //valid bishop moves
		/*      //bishop moves disabled because it caused some bugs in the game
		for(i=1;i<=7;i++){
			if(board[xfirst+i][yfirst-i]==0)
				moves[xfirst+i][yfirst-i]=1;
			else{
				if(blackWhite[xfirst+i][yfirst-i]==pcol)
					break;
				moves[xfirst+i][yfirst-i]=1;
				break;
			}
		}	
		for(i=1;i<=7;i++){
			if(board[xfirst+i][yfirst+i]==0)
				moves[xfirst+i][yfirst+i]=1;
			else{
				if(blackWhite[xfirst+i][yfirst+i]==pcol)
					break;
				moves[xfirst+i][yfirst+i]=1;
				break;
			}
		}
		for(i=1;i<=7;i++){
			if(board[xfirst-i][yfirst-i]==0)
				moves[xfirst-i][yfirst-i]=1;
			else{
				if(blackWhite[xfirst-i][yfirst-i]==pcol)
					break;
				moves[xfirst-i][yfirst-i]=1;
				break;
			}
		}
		for(i=1;i<=7;i++){
			if(board[xfirst-i][yfirst+i]==0)
				moves[xfirst-i][yfirst+i]=1;
			else{
				if(blackWhite[xfirst-i][yfirst+i]==pcol)
					break;
				moves[xfirst-i][yfirst+i]=1;
				break;
			}
		}
		*/
		for(i=0;i<8;i++){
			for(j=0;j<8;j++){
				moves[i][j]=1;   //bishop can move without restriction
			}
		}
		moves[xfirst][yfirst]=0;
	}
	
	if(id==4){   //valid knight moves
		for(i=-1;i<3;i+=2){
			for(j=-2;j<5;j+=4){
				if((((xfirst+i)<8)&&(((yfirst+j)<8)))&&(((xfirst+i)>-1)&&(((yfirst+j)>-1)))){
					if(blackWhite[xfirst+i][yfirst+j]!=pcol);
						moves[xfirst+i][yfirst+j]=1;
				}
				
				if((((xfirst+j)<8)&&(((yfirst+i)<8)))&&(((xfirst+j)>-1)&&(((yfirst+i)>-1)))){
					if(blackWhite[xfirst+j][yfirst+i]!=pcol);
						moves[xfirst+j][yfirst+i]=1;
				}
			}
		}
	}
	
	if(id==5){   //valid queeen moves
		/*		 //queen moves disabled because it caused some bugs in the game
		for(i=1;i<=xfirst;i++){
			if(board[xfirst-i][yfirst]==0)
				moves[xfirst-i][yfirst]=1;
			else{
				if(blackWhite[xfirst-i][yfirst]==pcol)
					break;
				moves[xfirst-i][yfirst]=1;
				break;
			}
		}	
		for(i=1;i<=(7-xfirst);i++){
			if(board[xfirst+i][yfirst]==0)
				moves[xfirst+i][yfirst]=1;
			else{
				if(blackWhite[xfirst+i][yfirst]==pcol)
					break;
				moves[xfirst+i][yfirst]=1;
				break;
			}
		}
		for(i=1;i<=yfirst;i++){
			if(board[xfirst][yfirst-i]==0)
				moves[xfirst][yfirst-i]=1;
			else{
				if(blackWhite[xfirst][yfirst-i]==pcol)
					break;
				moves[xfirst][yfirst-i]=1;
				break;
			}
		}
		for(i=1;i<=(7-yfirst);i++){
			if(board[xfirst][yfirst+i]==0)
				moves[xfirst][yfirst+i]=1;
			else{
				if(blackWhite[xfirst][yfirst+i]==pcol)
					break;
				moves[xfirst][yfirst+i]=1;
				break;
			}
		}
		
				for(i=1;i<=7;i++){
			if(board[xfirst+i][yfirst-i]==0)
				moves[xfirst+i][yfirst-i]=1;
			else{
				if(blackWhite[xfirst+i][yfirst-i]==pcol)
					break;
				moves[xfirst+i][yfirst-i]=1;
				break;
			}
		}	
		for(i=1;i<=7;i++){
			if(board[xfirst+i][yfirst+i]==0)
				moves[xfirst+i][yfirst+i]=1;
			else{
				if(blackWhite[xfirst+i][yfirst+i]==pcol)
					break;
				moves[xfirst+i][yfirst+i]=1;
				break;
			}
		}
		for(i=1;i<=7;i++){
			if(board[xfirst-i][yfirst-i]==0)
				moves[xfirst-i][yfirst-i]=1;
			else{
				if(blackWhite[xfirst-i][yfirst-i]==pcol)
					break;
				moves[xfirst-i][yfirst-i]=1;
				break;
			}
		}
		for(i=1;i<=7;i++){
			if(board[xfirst-i][yfirst+i]==0)
				moves[xfirst-i][yfirst+i]=1;
			else{
				if(blackWhite[xfirst-i][yfirst+i]==pcol)
					break;
				moves[xfirst-i][yfirst+i]=1;
				break;
			}
		}
		*/
		for(i=0;i<8;i++){
			for(j=0;j<8;j++){
				moves[i][j]=1;   //queen can move without restriction
			}
		}
		moves[xfirst][yfirst]=0;
	}
	if(id==6){   //valid king moves
		
		if(yfirst!=0){
			moves[xfirst][yfirst-1]=1;
			moves[xfirst-1][yfirst-1]=1;
			moves[xfirst+1][yfirst-1]=1;
			moves[xfirst+1][yfirst]=1;
			moves[xfirst-1][yfirst]=1;
		}
		if(yfirst!=7){
			moves[xfirst][yfirst+1]=1;
			moves[xfirst+1][yfirst+1]=1;
			moves[xfirst-1][yfirst+1]=1;
			moves[xfirst+1][yfirst]=1;
			moves[xfirst-1][yfirst]=1;
		}
		if(pcol==1){   //white king moves for castling
			if(xfirst==4){
				if(yfirst==7){
					if(board[7][7]==2){
						if((board[6][7]==0)&&(board[5][7]==0))
							legal=2;
					}
					if(board[0][7]==2){
						if((board[1][7]==0)&&(board[2][7]==0)){
							if(board[3][7]==0)
								legal=3;
						}
					}
				}
			}
		}
		if(pcol==2){   //black king moves for castling
			if(xfirst==3){
				if(yfirst==7){
					if(board[0][7]==2){
						if((board[1][7]==0)&&(board[2][7]==0))
							legal=2;
					}
					if(board[7][7]==2){
						if((board[6][7]==0)&&(board[5][7]==0)){
							if(board[4][7]==0)
								legal=3;
						}
					}
				}
			}
		}
	}
	
	if(moves[xsel][ysel])
		legal=1;   //if the move is valid
	
	return legal;   //returns the value of validity
}

void flipBoard(){		//flips the board, gets the symmetric w.r.t. origin
	int i,j,brd,blw;
	for(i=0;i<4;i++){
		for(j=0;j<8;j++){
			brd = board[i][j];
			blw = blackWhite[i][j];
			board[i][j] = board[7-i][7-j];
			blackWhite[i][j] = blackWhite[7-i][7-j];
			board[7-i][7-j] = brd;
			blackWhite[7-i][7-j] = blw;
		}
	}
}

void buttonpress(){   // makes a move if the move is valid or selects the piece 
	int i;
	int tempcol;

		if((moveFinish)%2==0){          // makes a move if the move is valid
			if((xsel!=xfirst)||(ysel!=yfirst)){
				tempo = board[xfirst][yfirst];			//stores the value of selected piece
				tempcol = blackWhite[xfirst][yfirst];   //stores the color of selected piece
				i=LegalMove();
				if((tempo)&&(i==1)){	//checks if the moving piece is a real piece and check if it is a legal move
					board[xfirst][yfirst]=0;//changing the board array in a desired way
					printsquare(xsel, ysel, 0, 0);		//clears the square that the piece will move		
					board[xsel][ysel] = tempo;			//writes stored piece to piece type array
					blackWhite[xsel][ysel] = tempcol;	//writes stored piece to piece color array
					blackWhite[xfirst][yfirst]=0;		//clears the old square of the piece
					
					drawPieces();	// draw all pieces again
					moveSound();	//play a sound after the move
				}
				if(i==2){		//same procedure for short castling 
					if(tempcol==1){
						board[xfirst+1][yfirst]=2;
						board[xfirst+3][yfirst]=0;
						blackWhite[xfirst+1][yfirst] = tempcol;
						blackWhite[xfirst+3][yfirst]=0;
					}
					if(tempcol==2){
						board[xfirst-1][yfirst]=2;
						board[xfirst-3][yfirst]=0;
						blackWhite[xfirst-1][yfirst] = tempcol;
						blackWhite[xfirst-3][yfirst]=0;
					}
					board[xsel][ysel] = 6;
					board[xfirst][yfirst]=0;
					
					blackWhite[xsel][ysel] = tempcol;
					blackWhite[xfirst][yfirst]=0;
					
					drawPieces();	
					moveSound();
				}
				if(i==3){		//same procedure for long castling 
					if(tempcol==1){
						board[xfirst-1][yfirst]=2;
						board[xfirst-4][yfirst]=0;
						blackWhite[xfirst-1][yfirst] = tempcol;
						blackWhite[xfirst-4][yfirst]=0;
					}
					if(tempcol==2){
						board[xfirst+1][yfirst]=2;
						board[xfirst+4][yfirst]=0;
						blackWhite[xfirst+1][yfirst] = tempcol;
						blackWhite[xfirst+4][yfirst]=0;
					}
					board[xsel][ysel] = 6;
					board[xfirst][yfirst]=0;
					
					blackWhite[xsel][ysel] = tempcol;
					blackWhite[xfirst][yfirst]=0;
					
					
					drawPieces();	
					moveSound();
				}
			}
			moveFinish+=1;
		}
		else	//selects the piece
			moveFinish+=1; //for the next button press make the move instead of selecting
}

void PS2_isr(void){   //interrupt handling
	int PS2_data, RVALID,oldadd,oldadd2;
	PS2_data = *(PS2_ptr); // read the Data register in the PS/2 port
	RVALID = PS2_data & 0x8000; // extract the RVALID field
	if (RVALID){
		
		int location,location2;

		bytey = clickbyte;		
		clickbyte = bytex;
		bytex = PS2_data & 0xFF;   //saves the last three bytes of data

		b++;
		if(b==3){   //every data pack contains three different serial bytes
			b=0;		
			if((clickbyte & 0x0E) == 0x08 ){   //this condition checks bit 1, 2, and 3 of data packs
				++a;						   //if bit3, bit2, and bit1 are 100, program consider it as byte 1
			}else{							   //this method works for at least seven right bit 1, 2, and three for error rate reduction
				a=0;						   //this method is used to recognize byte1, byte2, and byte3 in data packs
			}
			if(a==7){   //seeks for seven consecutive right data packs
				a=0;
				bytex = bytex & 0x07;   //scale down PS/2 bytes for error rate reduction
				bytey = bytey & 0x07;	//scale down PS/2 bytes for error rate reduction
				
				oldadd = (yloc<<8)+(xloc>>1);   	//stores old adress of cursor
				*(VGA+oldadd) = temp;				//writes the old color behind the cursor to the old adress
				oldadd2 = ((yloc+1)<<8)+(xloc>>1);  //stores old adress of cursor
				*(VGA+oldadd2) = temp2;				//writes the old color behind the cursor to the old adress
				
				
				if((clickbyte & 0x10) == 0x10 )   //keeps the x movement
					xloc += (-5);
				else
					xloc += 2*bytex;
				
				if((clickbyte & 0x20) == 0x20 )	  //keeps the y movement
					yloc -= (-5);
				else
					yloc -= 2*bytey;
				
				if(xloc >281)		//sets the boundaries of the cursor
					xloc=280;
				if(xloc<39)
					xloc=40;
				if(yloc > 240)
					yloc=239;
				if(yloc < 0)
					yloc = 1;		
						
				if((clickbyte & 0x01) == 0x01 ){	//detects if cursor clicked on a new square and saves old square 
					xfirst = xsel;
					yfirst = ysel;
					xsel = (xloc-40)/30;
					ysel = yloc/30;
								
					if(isDifferent()){
						buttonpress();		//if cursor clicked on a new square it goes to button press
					}
				}
					
				
				location=(yloc<<8)+(xloc>>1);
				temp = *(VGA+location);		//stores the old color behind the cursor to memory
				*(VGA+location) = 0x07E007E0;	//prints cursor to the new location
				location2=((yloc+1)<<8)+(xloc>>1);
				temp2 = *(VGA+location2);	//stores the old color behind the cursor to memory
				*(VGA+location2) = 0x07E007E0;  //prints cursor to the new location
				
			}
		}	


	}
	
		
}

void config_ps2(void){   //initialization of PS/2 port
	*(PS2_ptr) = 0xF4;
	*(PS2_ctrl) = 0x01;
	
}

void __attribute__ ((interrupt)) __cs3_isr_irq (void){
// Read the ICCIAR from the processor interface
int int_ID = *((int *) 0xFFFEC10C);
 //if (int_ID == 79) // check if interrupt is from the PS/2 port
PS2_isr ();
// Write to the End of Interrupt Register (ICCEOIR)
*((int *) 0xFFFEC110) = int_ID;
return;
}

// Define the remaining exception handlers */
void __attribute__ ((interrupt)) __cs3_isr_undef (void)
{
while (1);
}
void __attribute__ ((interrupt)) __cs3_isr_swi (void)
{
while (1);
}
void __attribute__ ((interrupt)) __cs3_isr_pabort (void)
{
while (1);
}
void __attribute__ ((interrupt)) __cs3_isr_dabort (void)
{
while (1);
}
void __attribute__ ((interrupt)) __cs3_isr_fiq (void)
{
while (1);
}

void printBoard(){   //prints the empty board
	int i=0;
	int j=0;
	int board[160];
	//Since each memory location (that can be accessed with each iteration in a loop with one increment)
		//...contains two pixels, x will be increased by two in the second loop when the board is printed
		//...thus the array should contain 320/2 = 160 elements. 
	int x=0;
	int y=0;
	int val=0;
	int color;

	for(i=0; i<160; i++){
		if(i<20)		//The first 20 pixels are black
			color=0;
		if(i==20)		//Start the board at the 40th pixel
			color=0xF695F695;
		if((i==35)||(i==50)||(i==65)||(i==80)||(i==95)||(i==110)||(i==125))
						//Swap the color of the squares each 30 pixel
			color=swapcolor(color);
		if(i>=140)		//The last 20 pixels are black
			color=0;
		board[j] = color;
		j++;
	}

	while (y<240){		//The screen is 240 pixels long
		*(VGA+val) = board[x];
		x+=1;
		if(x==160){
			x=0;
			y+=1; 
			if((y%30)==0){
				for(j=0; j<160; j++){
					board[j]=swapcolor(board[j]);
				}
			}
		}
		val=(y<<8)+(x);	   
		//This value should be added to the base address access each pixel
		//The base address is the address of the (0,0) pixel
	}
}


int main(){
	*HEX0 = 0;
	*HEX4 = 0;
	*SW = 0;
	int h3,h2,h1,h0;
	int turn=0;
	int increment=0;
	int res;
	int time_mode=0;
	int time[2];
	
	set_A9_IRQ_stack(); 	  //interrupt initializations
	config_GIC();			  //interrupt initializations
	enable_A9_interrupts();   //interrupt initializations
	config_ps2();		//PS/2 port initialization
	printBoard();		//prints the empty board
	drawPieces();		//draws the pieces on empty board
	while(1){			//Wait until the pushbutton is released to start the game
		if(*KEY&0x0F){			
		while(*KEY&0x0F);	
		break;				
		}
	}
	set_timemode(&increment, &time_mode); //Sets the time mode according to switches
	if(time_mode){						  //Initiates the total starting times for each player
		time [0] = time_mode;
		time [1] = time_mode;
		//Initiates the timer
		*timer_starthigh = 0x05F5;
		*timer_startlow	 = 0xE100;
		*timer_status  = 0x02;
		*timer_control = 0x07;
	}
	
while(1){
	while(1){	
		if(time_mode){
			//Time counts down each second until turn ends
			if((*timer_status & 0x01) == 1){
				*timer_status &= 0xFFFFFFFE;
				time[turn]-=1;
			}
			//Displays the time on 7SD
			res = turn+1;
			sevenSD(&res);
			h1 = (res<<8);
			*HEX4 = h1;
			h3 = time[turn]/600;
			sevenSD(&h3);
			h3 = h3<<24;
			h2 = (time[turn]/60)%10;
			sevenSD(&h2);
			h2 = h2<<16;
			h1 = (time[turn]%60)/10;
			sevenSD(&h1);
			h1 = h1<<8;
			h0 = time[turn]%10;
			sevenSD(&h0);
			h0 = h3+h2+h1+h0;
			*HEX0 = h0;	
		}
		if(*KEY&0x0F){			//Detects the pushbutton press
			while(*KEY&0x0F);	//Wait until pushbutton is released
			break;				//Get out of the loop to end the turn
		}
	}
	//Adds increment to the total time, flips the board and end the turn
		flipBoard();
		printBoard();
		drawPieces();
		time[turn] += increment;
		turn ^= 1;
}
	return 0;
}
