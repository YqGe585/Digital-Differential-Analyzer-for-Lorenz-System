///////////////////////////////////////
/// 640x480 version! 16-bit color
/// This code will segfault the original
/// DE1 computer
/// compile with
/// gcc graphics_video_16bit.c -o gr -O2 -lm
///
///////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/mman.h>
#include <sys/time.h> 
#include <math.h>
#include <unistd.h>
//#include "address_map_arm_brl4.h"

#define fix2int(a) (int)(a>>20)
#define fix2float(a) ((float)(a) / 1048576.0)
#define float2fix(a) (int)(a * 1048576.0)

// video display
#define SDRAM_BASE            0xC0000000
#define SDRAM_END             0xC3FFFFFF
#define SDRAM_SPAN			  0x04000000
// characters
#define FPGA_CHAR_BASE        0xC9000000 
#define FPGA_CHAR_END         0xC9001FFF
#define FPGA_CHAR_SPAN        0x00002000
/* Cyclone V FPGA devices */
#define HW_REGS_BASE          0xff200000
//#define HW_REGS_SPAN        0x00200000 
#define HW_REGS_SPAN          0x00005000 

// AXI Address
#define FPGA_AXI_BASE         0xC0000000
#define FPGA_AXI_SPAN         0x00001000

void *h2p_virtual_base;
volatile unsigned int * axi_pio_ptr = NULL;
volatile unsigned int * axi_pio_read_ptr = NULL;

//AXI LW Address
#define LW_AXI_BASE         0xff200000
#define LW_AXI_SPAN         0x00001000
#define PIO_X 0xB0
#define PIO_Y 0xC0
#define PIO_Z 0xD0
#define PIO_CLK 0x90
#define PIO_RESET 0xA0
#define PIO_X0 0x30
#define PIO_Y0 0x40
#define PIO_Z0 0x50
#define PIO_sigma 0x60
#define PIO_beta 0x70
#define PIO_rho 0x80
 
volatile unsigned int * lw_pio_ptr = NULL;
volatile unsigned int * lw_pio_read_ptr = NULL;
volatile signed int * my_pio_x_read_ptr = NULL;
volatile signed int * my_pio_y_read_ptr = NULL;
volatile signed int * my_pio_z_read_ptr = NULL;
volatile unsigned int * my_pio_reset_write_ptr = NULL;
volatile unsigned int * my_pio_clk_write_ptr = NULL;
volatile signed int * my_pio_x0_write_ptr = NULL;
volatile signed int * my_pio_y0_write_ptr = NULL;
volatile signed int * my_pio_z0_write_ptr = NULL;
volatile signed int * my_pio_sigma_write_ptr = NULL;
volatile signed int * my_pio_beta_write_ptr = NULL;
volatile signed int * my_pio_rho_write_ptr = NULL;

// graphics primitives
void VGA_text (int, int, char *);
void VGA_text_clear();
void VGA_box (int, int, int, int, short);
void VGA_rect (int, int, int, int, short);
void VGA_line(int, int, int, int, short) ;
void VGA_Vline(int, int, int, short) ;
void VGA_Hline(int, int, int, short) ;
void VGA_disc (int, int, int, short);
void VGA_circle (int, int, int, int);
// 16-bit primary colors
#define red  (0+(0<<5)+(31<<11))
#define dark_red (0+(0<<5)+(15<<11))
#define green (0+(63<<5)+(0<<11))
#define dark_green (0+(31<<5)+(0<<11))
#define blue (31+(0<<5)+(0<<11))
#define dark_blue (15+(0<<5)+(0<<11))
#define yellow (0+(63<<5)+(31<<11))
#define cyan (31+(63<<5)+(0<<11))
#define magenta (31+(0<<5)+(31<<11))
#define black (0x0000)
#define gray (15+(31<<5)+(51<<11))
#define white (0xffff)

char input_string[64];

int colors[] = {red, dark_red, green, dark_green, blue, dark_blue, 
		yellow, cyan, magenta, gray, black, white};

// pixel macro
#define VGA_PIXEL(x,y,color) do{\
	int  *pixel_ptr ;\
	pixel_ptr = (int*)((char *)vga_pixel_ptr + (((y)*640+(x))<<1)) ; \
	*(short *)pixel_ptr = (color);\
} while(0)

sem_t pause_sem;
sem_t resume_sem;

pthread_mutex_t lock;
pthread_cond_t cond;


// the light weight buss base
void *h2p_lw_virtual_base;

// pixel buffer
volatile unsigned int * vga_pixel_ptr = NULL ;
void *vga_pixel_virtual_base;

// character buffer
volatile unsigned int * vga_char_ptr = NULL ;
void *vga_char_virtual_base;

// /dev/mem file id
int fd;

// measure time
struct timeval t1, t2;
double elapsedTime;

volatile unsigned int timegap = 15000;
int paused = 0;
float temp = 0;

char text_top_row[40] = "LAB 1 LORENZ SYSTEM INTEGRATION\0";
char text_next[40] = "yg585, kg534, sj778\0";
char text_1[40] = "x0:\0";
char text_2[40] = "y0:\0";
char text_3[40] = "z0:\0";
char text_4[40] = "sigma:\0";
char text_5[40] = "beta:\0";
char text_6[40] = "rho:\0";

void textprint()
{
	VGA_text_clear();
	sprintf(text_1, "x0=%.2f",fix2float(*(my_pio_x0_write_ptr)));
	sprintf(text_2, "y0=%.2f",fix2float(*(my_pio_y0_write_ptr)));
	sprintf(text_3, "z0=%.2f",fix2float(*(my_pio_z0_write_ptr)));
	sprintf(text_4, "sigma=%.2f",fix2float(*(my_pio_sigma_write_ptr)));
	sprintf(text_5, "beta=%.2f", fix2float(*(my_pio_beta_write_ptr)));
	sprintf(text_6, "rho=%.2f",fix2float(*(my_pio_rho_write_ptr)));
	VGA_text (1, 50, text_top_row);
	VGA_text (1, 51, text_next);
	VGA_text (1, 52, text_1);
	VGA_text (1, 53, text_2);
	VGA_text (1, 54, text_3);
	VGA_text (1, 55, text_4);
	VGA_text (1, 56, text_5);
	VGA_text (1, 57, text_6);
}


void * userinput()
{
	while(1)
	{
		printf("command:");
		scanf("%s",input_string);
		if(strcmp(input_string,"x0") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_x0_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"y0") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_y0_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"z0") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_z0_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"sigma") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_sigma_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"rho") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_rho_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"beta") == 0)
		{
			scanf("%f", &temp);
			pthread_mutex_lock(&lock);
			*(my_pio_beta_write_ptr) = float2fix(temp);
		}
		else if(strcmp(input_string,"time") == 0)
		{
			scanf("%d", &timegap);
			pthread_mutex_lock(&lock);
		}
		else if(strcmp(input_string,"pause") == 0)
		{
			paused = 1;
		}
		else if(strcmp(input_string,"resume") == 0)
		{
			paused = 0;
            pthread_cond_broadcast(&cond);
		}
		else if(strcmp(input_string,"reset") == 0)
		{
			paused = 1;
			VGA_box (0, 0, 639, 479, 0x0000);
			*(my_pio_reset_write_ptr) = 0;
			*(my_pio_reset_write_ptr) = 0xFFFFFFFF;
			*(my_pio_clk_write_ptr) = 0;
			*(my_pio_clk_write_ptr) = 0xFFFFFFFF;
			*(my_pio_clk_write_ptr) = 0;
			*(my_pio_reset_write_ptr) = 0;
		}
		else if(strcmp(input_string,"clear") == 0)
		{
			VGA_box (0, 0, 639, 479, 0x0000);
			*(my_pio_reset_write_ptr) = 0;
			*(my_pio_reset_write_ptr) = 0xFFFFFFFF;
			*(my_pio_clk_write_ptr) = 0;
			*(my_pio_clk_write_ptr) = 0xFFFFFFFF;
			*(my_pio_clk_write_ptr) = 0;
			*(my_pio_reset_write_ptr) = 0;
		}
		textprint();
		pthread_mutex_unlock(&lock);
	}
}

void * HPS_output()
{
	while(1)
	{
		pthread_mutex_lock(&lock);
		while (paused) {
            pthread_cond_wait(&cond, &lock);
        }
		pthread_mutex_unlock(&lock);
		*(my_pio_clk_write_ptr) = 0;
		*(my_pio_clk_write_ptr) = 0xFFFFFFFF;
		VGA_PIXEL( (140+(int)(4*fix2float(*(my_pio_x_read_ptr)))),(80+(int)(4*fix2float(*(my_pio_z_read_ptr)))),green );
		VGA_PIXEL( (480+(int)(4*fix2float(*(my_pio_y_read_ptr)))),(80+(int)(4*fix2float(*(my_pio_z_read_ptr)))),red );
		VGA_PIXEL( (300+(int)(4*fix2float(*(my_pio_x_read_ptr)))),(340+(int)(4*fix2float(*(my_pio_y_read_ptr)))),yellow );
		usleep(timegap);
	}
}
	
int main(void)
{
  	
	// === need to mmap: =======================
	// FPGA_CHAR_BASE
	// FPGA_ONCHIP_BASE      
	// HW_REGS_BASE        
  
	// === get FPGA addresses ==================
    // Open /dev/mem
	if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) 	{
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}
    
    // get virtual addr that maps to physical
	h2p_lw_virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );	
	if( h2p_lw_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap1() failed...\n" );
		close( fd );
		return(1);
	}
    lw_pio_ptr = (unsigned int*) h2p_lw_virtual_base;
	my_pio_x_read_ptr = (signed int*) (h2p_lw_virtual_base+PIO_X);
	my_pio_y_read_ptr = (signed int*) (h2p_lw_virtual_base+PIO_Y);
	my_pio_z_read_ptr = (signed int*) (h2p_lw_virtual_base+PIO_Z);
	my_pio_reset_write_ptr = (unsigned int*) (h2p_lw_virtual_base+PIO_RESET);
	my_pio_clk_write_ptr = (unsigned int*) (h2p_lw_virtual_base+PIO_CLK);
	my_pio_x0_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_X0);
	my_pio_y0_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_Y0);
	my_pio_z0_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_Z0);
	my_pio_sigma_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_sigma);
	my_pio_beta_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_beta);
	my_pio_rho_write_ptr = (signed int*) (h2p_lw_virtual_base+PIO_rho);

	// === get VGA char addr =====================
	// get virtual addr that maps to physical
	vga_char_virtual_base = mmap( NULL, FPGA_CHAR_SPAN, ( 	PROT_READ | PROT_WRITE ), MAP_SHARED, fd, FPGA_CHAR_BASE );	
	if( vga_char_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap2() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA LED control 
	vga_char_ptr =(unsigned int *)(vga_char_virtual_base);

	// === get VGA pixel addr ====================
	// get virtual addr that maps to physical
	vga_pixel_virtual_base = mmap( NULL, SDRAM_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, SDRAM_BASE);	
	if( vga_pixel_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap3() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA pixel buffer
	vga_pixel_ptr =(unsigned int *)(vga_pixel_virtual_base);

	// ===========================================

	/* create a message to be displayed on the VGA 
          and LCD displays */

	char num_string[20], time_string[20] ;
	char color_index = 0 ;
	int color_counter = 0 ;
	
	// position of disk primitive
	int disc_x = 0;
	// position of circle primitive
	int circle_x = 0 ;
	// position of box primitive
	int box_x = 5 ;
	// position of vertical line primitive
	int Vline_x = 350;
	// position of horizontal line primitive
	int Hline_y = 250;

	//VGA_text (34, 1, text_top_row);
	//VGA_text (34, 2, text_bottom_row);
	// clear the screen
	VGA_box (0, 0, 639, 479, 0x0000);
	// clear the text
	// VGA_text_clear();
	// // write text
	// VGA_text (1, 50, text_top_row);
	// VGA_text (1, 51, text_next);
	textprint();


	*(my_pio_x0_write_ptr) = float2fix(-1);
	*(my_pio_y0_write_ptr) = float2fix(0.1);
	*(my_pio_z0_write_ptr) = float2fix(25);
	*(my_pio_sigma_write_ptr) = float2fix(10);
	*(my_pio_beta_write_ptr) = float2fix(8/3);
	*(my_pio_rho_write_ptr) = float2fix(28);



	*(my_pio_reset_write_ptr) = 0;
	*(my_pio_reset_write_ptr) = 0xFFFFFFFF;
	*(my_pio_clk_write_ptr) = 0;
	*(my_pio_clk_write_ptr) = 0xFFFFFFFF;
	*(my_pio_clk_write_ptr) = 0;
	*(my_pio_reset_write_ptr) = 0;
	
	// R bits 11-15 mask 0xf800
	// G bits 5-10  mask 0x07e0
	// B bits 0-4   mask 0x001f
	// so color = B+(G<<5)+(R<<11);
	
	pthread_t tid_userinput, tid_HPS_output;


    sem_init(&pause_sem, 0, 0); 
    sem_init(&resume_sem, 0, 1); //resume is ready

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&tid_userinput, NULL, userinput, NULL);
    pthread_create(&tid_HPS_output, NULL, HPS_output, NULL);

    pthread_join(tid_userinput, NULL);
    pthread_join(tid_HPS_output, NULL);

    return 0;
} // end main

/****************************************************************************************
 * Subroutine to send a string of text to the VGA monitor 
****************************************************************************************/
void VGA_text(int x, int y, char * text_ptr)
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset;
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	while ( *(text_ptr) )
	{
		// write to the character buffer
		*(character_buffer + offset) = *(text_ptr);	
		++text_ptr;
		++offset;
	}
}

/****************************************************************************************
 * Subroutine to clear text to the VGA monitor 
****************************************************************************************/
void VGA_text_clear()
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset, x, y;
	for (x=0; x<79; x++){
		for (y=0; y<59; y++){
	/* assume that the text string fits on one line */
			offset = (y << 7) + x;
			// write to the character buffer
			*(character_buffer + offset) = ' ';		
		}
	}
}

/****************************************************************************************
 * Draw a filled rectangle on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_box(int x1, int y1, int x2, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
	if (x1>x2) SWAP(x1,x2);
	if (y1>y2) SWAP(y1,y2);
	for (row = y1; row <= y2; row++)
		for (col = x1; col <= x2; ++col)
		{
			//640x480
			//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
			// set pixel color
			//*(char *)pixel_ptr = pixel_color;	
			VGA_PIXEL(col,row,pixel_color);	
		}
}

/****************************************************************************************
 * Draw a outline rectangle on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_rect(int x1, int y1, int x2, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
	if (x1>x2) SWAP(x1,x2);
	if (y1>y2) SWAP(y1,y2);
	// left edge
	col = x1;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
		
	// right edge
	col = x2;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
	
	// top edge
	row = y1;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);
	}
	
	// bottom edge
	row = y2;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);
	}
}

/****************************************************************************************
 * Draw a horixontal line on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_Hline(int x1, int y1, int x2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (x1>x2) SWAP(x1,x2);
	// line
	row = y1;
	for (col = x1; col <= x2; ++col){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);		
	}
}

/****************************************************************************************
 * Draw a vertical line on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_Vline(int x1, int y1, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (y2<0) y2 = 0;
	if (y1>y2) SWAP(y1,y2);
	// line
	col = x1;
	for (row = y1; row <= y2; row++){
		//640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;	
		VGA_PIXEL(col,row,pixel_color);			
	}
}


/****************************************************************************************
 * Draw a filled circle on the VGA monitor 
****************************************************************************************/

void VGA_disc(int x, int y, int r, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col, rsqr, xc, yc;
	
	rsqr = r*r;
	
	for (yc = -r; yc <= r; yc++)
		for (xc = -r; xc <= r; xc++)
		{
			col = xc;
			row = yc;
			// add the r to make the edge smoother
			if(col*col+row*row <= rsqr+r){
				col += x; // add the center point
				row += y; // add the center point
				//check for valid 640x480
				if (col>639) col = 639;
				if (row>479) row = 479;
				if (col<0) col = 0;
				if (row<0) row = 0;
				//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
				// set pixel color
				//*(char *)pixel_ptr = pixel_color;
				VGA_PIXEL(col,row,pixel_color);	
			}
					
		}
}

/****************************************************************************************
 * Draw a  circle on the VGA monitor 
****************************************************************************************/

void VGA_circle(int x, int y, int r, int pixel_color)
{
	char  *pixel_ptr ; 
	int row, col, rsqr, xc, yc;
	int col1, row1;
	rsqr = r*r;
	
	for (yc = -r; yc <= r; yc++){
		//row = yc;
		col1 = (int)sqrt((float)(rsqr + r - yc*yc));
		// right edge
		col = col1 + x; // add the center point
		row = yc + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
		// left edge
		col = -col1 + x; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
	}
	for (xc = -r; xc <= r; xc++){
		//row = yc;
		row1 = (int)sqrt((float)(rsqr + r - xc*xc));
		// right edge
		col = xc + x; // add the center point
		row = row1 + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
		// left edge
		row = -row1 + y; // add the center point
		//check for valid 640x480
		if (col>639) col = 639;
		if (row>479) row = 479;
		if (col<0) col = 0;
		if (row<0) row = 0;
		//pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
		// set pixel color
		//*(char *)pixel_ptr = pixel_color;
		VGA_PIXEL(col,row,pixel_color);	
	}
}

// =============================================
// === Draw a line
// =============================================
//plot a line 
//at x1,y1 to x2,y2 with color 
//Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void VGA_line(int x1, int y1, int x2, int y2, short c) {
	int e;
	signed int dx,dy,j, temp;
	signed int s1,s2, xchange;
     signed int x,y;
	char *pixel_ptr ;
	
	/* check and fix line coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
        
	x = x1;
	y = y1;
	
	//take absolute value
	if (x2 < x1) {
		dx = x1 - x2;
		s1 = -1;
	}

	else if (x2 == x1) {
		dx = 0;
		s1 = 0;
	}

	else {
		dx = x2 - x1;
		s1 = 1;
	}

	if (y2 < y1) {
		dy = y1 - y2;
		s2 = -1;
	}

	else if (y2 == y1) {
		dy = 0;
		s2 = 0;
	}

	else {
		dy = y2 - y1;
		s2 = 1;
	}

	xchange = 0;   

	if (dy>dx) {
		temp = dx;
		dx = dy;
		dy = temp;
		xchange = 1;
	} 

	e = ((int)dy<<1) - dx;  
	 
	for (j=0; j<=dx; j++) {
		//video_pt(x,y,c); //640x480
		//pixel_ptr = (char *)vga_pixel_ptr + (y<<10)+ x; 
		// set pixel color
		//*(char *)pixel_ptr = c;
		VGA_PIXEL(x,y,c);			
		 
		if (e>=0) {
			if (xchange==1) x = x + s1;
			else y = y + s2;
			e = e - ((int)dx<<1);
		}

		if (xchange==1) y = y + s2;
		else x = x + s1;

		e = e + ((int)dy<<1);
	}
}