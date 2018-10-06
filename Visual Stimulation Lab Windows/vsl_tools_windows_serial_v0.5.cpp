#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <math.h>			// Needed for movement speed calculations.
#include <getopt.h>
#include "engine.h"
#include "matrix.h"
#define checkImageWidth 128	// Width of openGL created visual gradient.
#define checkImageHeight 8	// Height of openGL created visual gradient.
#define baudrate 1382400	// Initial baud rate for any serial communication.
// win32 change = remove "B" prefix for baudrate

int runMode = 0; 				// 1 = Movement/Active Mode, 0 = Still/Stop Mode
int displayMode = 1;			// 1 = verticalBar, 2 = sineGradient, 3 = horizontalBar
float shift = 0;				// Initial value for how much the world is "shifted" (object movement).
float gain = 0.4;				// How much the pattern is shifted by.
float gainShift = 0.2;			// Added to gain (how much pattern is shifted) to increase speed w/up&down keys.
float speed = 1;				// Initial speed multiplier
float size = 1;					// Initial size multiplier.
int sizeMod = 1;			// For w(ide) and t(iny), this is how much it increases/decreases by.
int sizeOrig = 8;				// How big the patterns are originally.
unsigned char serialbuffer[1];	// Where serial data is stored/read into.
int fd = 0;						// Serial port.
int rawTorque = 0;				// Raw torque value (0-255)
int window_width = 480;			// Set initial window width.
int window_height = 640;		// Set initial window height.
float curve = 2;				// Curvature of the projection.
int bias = 0;					// Initial bias value.
int biasMod = 1;				// How much bias is shifted by.
int dataCounter = 0;			// For counting numberOfSamples.
int numberOfSamples = 3000;		// Number of data points taken and sent to MATLAB.
int recordMode = 0;				// 0 = No recording, 1 = Recording.
int oscillation = 0;
int oscillationFlag = 0;
int oscillationMode = 0;

Engine *ep;						// Initializes MATLAB engine.
mxArray *cppVisualSamples;		// MATLAB array for visual position of object.
mxArray *cppTimingSamples;		// MATLAB array for timing information.
double *dataSetVisual;			// Pointer for visual information.
double *dataSetTiming;			// Pointer for timing information.
struct termios toptions;		// Serial port options.
static GLubyte checkImage[checkImageHeight][checkImageWidth][4];
static GLuint texName;

int serialport_init(const char* serialport, int baud);				// Initializes the serial port, sets the baud rate.
int serialport_read(int fd, unsigned char* serialbuffer);			// Reads a byte of data into serialbuffer[].

/* Oscillation mode (no torque input, used to measure raw torque response). */
void oscillationFn() {
	if (oscillationFlag == 1) {
		oscillation=oscillation-gain;
		if (oscillation <= 0) {
			oscillationFlag = 0;
		}
	} else {
		oscillation=oscillation+gain;
		if (oscillation >= (window_width-(sizeOrig*size))) {
			oscillationFlag = 1;
		}
	}
}

/* Wraps single objects around to the other side of the screen. */
void shiftWrap() {
	if (shift > window_width+(sizeOrig*size)) {		
		shift = -(sizeOrig*size);	
	} else if (shift < -(sizeOrig*size)) { 
		shift = (window_width+(sizeOrig*size));	
	}
}

/* Defines the shifting and speed of the visual image. */
void speedControl() {
	serialport_read(fd, serialbuffer);				// Grabs the next byte from usbserial.
	speed = fabs(127-(rawTorque+bias));				// Absolute value of how far away from the midline you are.
	speed = pow((speed/50),2);					// (how far from the midline you are / 50)^2
	if ((rawTorque+bias)<127) {
		shift=shift+(gain*speed);
	} else {
		shift=shift-(gain*speed);
	}
	shiftWrap();
}

/* Gathers second and millisecond data for MATLAB processing. */
void print_time () {
	struct timeval tv;
	struct tm* ptm;
	char time_string[10];
	float milliseconds;
	float secAndMs;
	
	gettimeofday(&tv, NULL);										// Obtain the time of day, and convert it to a tm struct.
	ptm = localtime(&tv.tv_sec);
	strftime(time_string, sizeof(time_string), "%S", ptm);			// Format the date and time, down to a single second.
	milliseconds = tv.tv_usec / 1000;								// Compute milliseconds from microseconds.
	secAndMs = strtol(time_string,NULL,10) + (milliseconds/1000);	// Tag it on w/the milliseconds
	dataSetTiming[dataCounter] = secAndMs;							// Save it for MATLAB
	dataCounter++;
}

/* Record data for MATLAB processing. */
void recordData() {
	if (dataCounter<numberOfSamples) {
		dataSetVisual[dataCounter] = shift+((sizeOrig*size)/2);
		print_time();
	} else if (dataCounter == numberOfSamples) {
		engPutVariable(ep, "cppVisualSamples", cppVisualSamples); 
		engPutVariable(ep, "cppTimingSamples", cppTimingSamples);
		engEvalString(ep, "save visualdata.mat");
		fprintf(stderr, "Recording of %i samples is complete.  Data saved as \"visualdata.mat\".\n", numberOfSamples);
		recordMode = 0;
		dataCounter++;
	}
}

/* Simple line movement controlled via serialport_read input; speed is controlled via shifter function. */
void horizontalBar() {
	if (oscillationMode == 1) {
		oscillationFn();
		glTranslatef(oscillation,window_height/2,0);		// shifts along x-axis
	} else {
		glTranslatef(shift,window_height/2,0);				// shifts along x-axis
	}
	
	int corner = 0;
	int x2 = sizeOrig*size;
	int y2 = window_height/30;

	glBegin(GL_QUADS);
		glColor3f(0.0f, 1.0f, 0.0f);			// Green
		//glColor3f(0.0f, 0.0f, 1.0f);			// Blue
		//glColor3f(1.0f, 0.0f, 0.0f);			// Red
		//glColor3f(1.0f, 1.0f, 1.0f);			// White
		glVertex2f(corner,corner);
		glVertex2f(x2,corner);
		glVertex2f(x2,y2);
		glVertex2f(corner,y2);
	glEnd();
}

/* Rendering function for the sine gradient. */
void sineGradient() {
	glTranslatef(window_width/2,window_height,0);		// Shift along the x axis.
	glRotatef(90,1.0,0,0);				// Rotates the cylinder to face the screen.
	if (oscillationMode == 1) {
		oscillationFn();
		glRotatef(oscillation+gain,0,0,1.0);
	} else {
		glRotatef(shift,0,0,1.0);
	}
	
	GLUquadric *myQuad;
	myQuad=gluNewQuadric();
	glEnable(GL_TEXTURE_2D);
	gluQuadricTexture(myQuad, GL_TRUE);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(10.0*size,1.0,1.0);
	//gluCylinder(quadric, base radius, top radius, height, slices, stacks);
	gluCylinder(myQuad, window_width/curve, window_width/curve, window_height, 256, 2);
	glDisable(GL_TEXTURE_2D);
}

/* Simple line movement controlled via serialport_read input; speed is controlled via shifter function. */
void verticalBar() {
	if (oscillationMode == 1) {
		oscillationFn();
		glTranslatef(oscillation, 0.0, 0.0);					// Shift along the x axis.
	} else {
		glTranslatef(shift, 0.0, 0.0);					// Shift along the x axis.
	}
	
	int corner = 0;
	int x2 = sizeOrig*size;
	int y2 = window_height;
	
	glBegin(GL_QUADS);
		//glColor3f(0.0f, 1.0f, 0.0f);			// Green
		//glColor3f(1.0f, 0.0f, 0.0f);			// Red
		glColor3f(0.0f, 0.0f, 1.0f);			// Blue
		//glColor3f(0.5f, 0.0f, 1.0f);			// Purple
		glVertex2f(corner,corner);
		glVertex2f(x2,corner);
		glVertex2f(x2,y2);
		glVertex2f(corner,y2);
	glEnd();
}

void solidCircle() {
	if (oscillationMode == 1) {
		oscillationFn();
		glTranslatef(oscillation, window_height/2, 0.0);		// Shift along the x axis.
	} else {
		glTranslatef(shift, window_height/2, 0.0);				// Shift along the x axis.
	}
	
	int angle = 0;
	int k1 = 0;
	glBegin(GL_TRIANGLE_FAN);
		glColor3f(0.0f, 1.0f, 0.0f);							// Green
		glVertex2f(k1, k1);
		for (angle = 0; angle <= 360; angle++) {
			glVertex2f(k1 + sin(angle) * (sizeOrig*size), k1 + cos(angle) * (sizeOrig*size));
		}
	glEnd();
}

/* myKeyboardFunc():  Keyboard input menu.  */
void myKeyboardFunc( unsigned char key, int x, int y ) {
	switch ( key ) {
	case 'f':
		glutFullScreen();
		break;
	case 'b':
		fprintf(stderr, "[b]:  Current torque value is:  %i.\n", rawTorque);
		break;
	case 'g':
		fprintf(stderr, "[g]:  Active torque mode.  Press \"s\" to stop.\n");
		runMode = 1;
		glutPostRedisplay();
		break;
	case 's':
		fprintf(stderr, "[s]:  Stop mode.  Press \"g\" to run.\n");
		oscillationMode = 0;
		runMode = 0;
		break;
	case 'r':
		fprintf(stderr, "[r]:  Recording %i samples.\n", numberOfSamples);
		fprintf(stderr, "Current displayMode is:  %i.\n", displayMode);
		fprintf(stderr, "Current size is:  %.3f.\n", size);
		fprintf(stderr, "Current curvature of the display is:  %.3f.\n", curve);
		fprintf(stderr, "Current bias is:  %i.\n", bias);
		fprintf(stderr, "Current gain is:  %.3f.\n\n", gain);
		recordMode = 1;
		dataCounter = 0;
		runMode = 1;
		glutPostRedisplay();
		break;
	case 'w':
		fprintf(stderr, "[w]:  Increasing width by %i, size is now: %.3f.\n", sizeMod, size);
		size = size+sizeMod;
		glutPostRedisplay();
		break;
	case 't':
		fprintf(stderr, "[t]:  Decreasing width by %i, size is now: %.3f.\n", sizeMod, size);
		size = size-sizeMod;
		glutPostRedisplay();
		break;
	case 'c':
		fprintf(stderr, "[c]:  Increasing the curvature of the display to: %.3f).\n", curve);
		curve = curve+0.5;
		glutPostRedisplay();
		break;
	case 'p':
		fprintf(stderr, "[p]:  Decreasing the curvature of the display to: %.3f).\n.", curve);
		curve = curve-0.5;
		glutPostRedisplay();
		break;
	case '0':
		fprintf(stderr, "[0]:  Setting displayMode to 0 (blank screen).\n");
		displayMode = 0;
		glutPostRedisplay();
		break;
	case '1':
		fprintf(stderr, "[1]:  Setting displayMode to 1 (verticalBar).\n");
		displayMode = 1;
		shift = 0;
		glutPostRedisplay();
		break;
	case '2':
		fprintf(stderr, "[2]:  Setting displayMode to 2 (sineGradient).\n");
		displayMode = 2;
		shift = 0;
		glutPostRedisplay();
		break;
	case '3':
		fprintf(stderr, "[3]:  Setting displayMode to 3 (horizontalBar).\n");
		displayMode = 3;
		shift = 0;
		glutPostRedisplay();
		break;
	case '4':
		fprintf(stderr, "[4]:  Setting displayMode to 4 (solidCircle).\n");
		displayMode = 4;
		shift = 0;
		glutPostRedisplay();
		break;
	case '.':
		bias=bias+biasMod;
		fprintf(stderr, "[.]:  Increasing clockwise bias to: %i.\n",bias);
		break;
	case ',':
		bias=bias-biasMod;
		fprintf(stderr, "[,]:  Increasing counterclockwise bias to:  %i.\n",bias);
		break;
	case 'k':
		fprintf(stderr, "[k]:  Resetting bias and gain.\n");
		gain = 0.6;
		bias = 0;
		break;
	case 'o':
		fprintf(stderr, "[o]:  Starting oscillation mode (no torque input).\n");
		runMode = 1;
		oscillationMode = 1;
		break;
	case 'q':
		exit(1);
	case 27:
		glutReshapeWindow(window_width,window_height);
		glutPositionWindow(100,100);
		break;
	}
}

/* Keyboard input menu for special keys (up and down):  controls gain. */
void mySpecialKeyFunc( int key, int x, int y ) {
	switch ( key ) {
	case GLUT_KEY_UP:
		if (gain<10) {				// Avoid overflow problems.
			gain=gain+gainShift;
		}
		fprintf(stderr, "[Up]:  Increasing gain by %.3f, gain is now: %.3f.\n",gainShift,gain);
		break;
	case GLUT_KEY_DOWN:
		if (gain>0.001) {			// Avoid underflow problems.
			gain=gain-gainShift;
		}
		fprintf(stderr, "[Down]:  decreasing gain by %.3f, gain is now: %.3f.\n",gainShift,gain);
		break;
	}
}

/* Initialization function -- runs once to define the visual field. */
void init() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glOrtho(0,window_width,window_height,0,-window_width,window_width);
}

/* Main glutDisplayLoop; controls the higher level GL functions between the rendering functions. */
void display(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//speedControl();
	if (displayMode == 1) {
		verticalBar();
	} else if (displayMode == 2) {
		sineGradient();
	} else if (displayMode == 3) {
		horizontalBar();
	} else if (displayMode == 4) {
		solidCircle();
	}
	glPopMatrix();
	glutSwapBuffers();
}

void idleDisplay() {
	speedControl();
	if (displayMode == 1) {
		verticalBar();
	} else if (displayMode == 2) {
		sineGradient();
	} else if (displayMode == 3) {
		horizontalBar();
	} else if (displayMode == 4) {
		solidCircle();
	}
	if (recordMode == 1) {
		recordData();
	}
	if (runMode == 1) {
		glutPostRedisplay();
	}
}

/* Creates the actual texture map for use with sineGradient().  checkImage[] holds pixels w/RGBA values.
 * To achieve colors, change one of the R/G/B values accordingly. */
void makeCheckImage(void) {
   int i, j, c;
   for (i = 0; i < checkImageHeight; i++) {
      for (j = 0; j < checkImageWidth; j++) {
        //c = ((((i&0x8)==0)^((j&0x8))==0))*255;		// Alternating checker pattern.
		if (j<64) {
			c=(4*j);
		} else {
			c=511-(4*j);
		}
		checkImage[i][j][0] = (GLubyte) c;			// R
        checkImage[i][j][1] = (GLubyte) c;			// G
        checkImage[i][j][2] = (GLubyte) c;			// B
        checkImage[i][j][3] = (GLubyte) 255;		// Alpha
      }
   }
}

int main(int argc, char **argv) {
	char serialport[256];
    int n;

    if (argc==1) {
        exit(EXIT_SUCCESS);
    }

    // Parse options from the command line.
    int option_index = 0, opt;
    static struct option loptions[] = {
    	{"port", required_argument, 0, 'p'},		// Command line for setting port: -p /dev/tty.usbmodem####
	    {"delay", required_argument, 0, 'd'}		// Command line for zero delay: -d 0
	};

    while(1) {
    	opt = getopt_long(argc, argv, "hp:b:s:rn:d:", loptions, &option_index);
    	if (opt==-1) break;
    	switch (opt) {
			case '0': break;
	        case 'd':
	            n = strtol(optarg,NULL,10);
	            usleep(n * 1000); 									// Delay (sleep in milliseconds).
	            break;
	        case 'p':
	            strcpy(serialport,optarg);
	            fd = serialport_init(optarg, baudrate);
	            if(fd==-1) return -1;
	            break;
    	}
    }
	
	// Opens MATLAB engine, creates matrices & pointers for visual and timing samples.
	ep = engOpen(NULL);
	cppVisualSamples = mxCreateDoubleMatrix(numberOfSamples, 1, mxREAL);
	cppTimingSamples = mxCreateDoubleMatrix(numberOfSamples, 1, mxREAL);
	dataSetVisual = mxGetPr(cppVisualSamples);
	dataSetTiming = mxGetPr(cppTimingSamples);
    glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(window_width,window_height);					// Window size (in pixels).
	glutInitWindowPosition(100,100);								// Window position (from upper left).
	glutCreateWindow("Torquometer Visual Field");					// Window title.
	init();
	glutKeyboardFunc(myKeyboardFunc);								// Handles "normal" ascii symbols (letters).
	glutSpecialFunc(mySpecialKeyFunc);								// Handles "special" keyboard keys (up, down).
	glutDisplayFunc(display);										// Default display mode.
	glutIdleFunc(idleDisplay);										// Used with glutSwapBuffers();
	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_TEXTURE_2D);
	makeCheckImage();												// Generates the gradient pattern in the background.
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, checkImageWidth, checkImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, checkImage);
	glutMainLoop();
}

/* Reads a single byte of input from the serial port. */
int serialport_read(int fd, unsigned char* serialbuffer) {
	int n = read(fd, serialbuffer, 1);
	if (n==-1) {
		return -1; 						// Couldn't read.
		fprintf(stderr, "error:  couldn't read from the serial port.");
	} else if (n==0) {
		usleep(10*1000); 				// No data available.  Wait 10 ms to try again.
		fprintf(stderr, "error:  no data available (flooding and/or finished).  Stopping.\n");
		displayMode = 0;
		runMode = 0;
		glutPostRedisplay();
		return 0;
	} else {
		rawTorque = serialbuffer[0];
		tcsetattr(fd, TCSAFLUSH, &toptions);
		return 0;
	}
}

/* Takes the string name of the serial port (e.g. "/dev/tty.usbserial","COM1") and a baud rate (bps).
 * Opens the port in fully raw mode, returns valid fd, or -1 on error. */
int serialport_init(const char* serialport, int baud) {
    int fd;
    fprintf(stderr, "init_serialport: opening port %s @ %i bps\n", serialport, baud);

    /* Opens the serial port w/the following options:
     * (disabled) O_RDWR flag:	   read/write mode
     * O_RDONLY:  		read only mode
     * O_NOCTTY flag:  	tells UNIX that this program doesn't want to be the "controlling terminal" for that port
     * O_NDELAY flag:  	tells UNIX that this program doesn't care what state the DCD signal line is in -
     * 					whether the other end of the port is up and running. */
    fd = open(serialport, O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        return -1;
    }
	
    speed_t brate = baud;

    // Sets the baudrate to whatever baudrate in main() is set to.
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);

    /* cflag = control options
     * iflag = input options
     * lflag = line options
     * oflag = output options */
	
    // No parity (8N1).
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;

    // No flow control.
    toptions.c_cflag &= ~CRTSCTS;

    // Turn on READ & ignore control lines.
    toptions.c_cflag |= CREAD | CLOCAL;

    // Turn off s/w flow control.
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Raw.
    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    toptions.c_oflag &= ~OPOST; 

    // See: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 1;
    toptions.c_cc[VTIME] = 10;
	
    return fd;
}
