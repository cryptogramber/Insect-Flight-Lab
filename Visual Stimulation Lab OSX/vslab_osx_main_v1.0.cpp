/* Visual Stimulation Laboratory OSX V1.0, Amber Fechko, amber@kairozu.com
 *
 * Yes, this code is messy.  Amber:Programmer::"Anyone that can make a grilled cheese":Chef  
 * (Suggestions welcomed.)
 *
 * This is a continuation of the Torque Tools V6 code using direct NI DAQ access.
 *
 * To do:
 * Figure out any timing delays in the new system.
 * Do some torque test runs.
 * Create a GUI (nevermind. slow.)
 * Make it more sensitive than 255.
 *
 */

#include <stdlib.h>
#include <GLUT/glut.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>					// Needed for movement speed calculations.
#include <getopt.h>
#include <engine.h>					// MATLAB header file.
#include <matrix.h>
#include "NIDAQmxBase.h"				// NI DAQ header file (download from ni.com).
#include <stdio.h>

#define 	checkImageWidth 128			// Width of openGL created visual gradient.
#define 	checkImageHeight 8			// Height of openGL created visual gradient.
#define 	bufferSize (uInt32)1			// Buffer size for reading from DAQ board.

TaskHandle 	taskHandle = 0;				// For NIDAQmxBase handlers.
int32 		pointsRead = 0;				// How many data points have been read.
int32 		error = 0;				// Error function.
char 		errBuff[2048]={'\0'};			// Error data buffer.
float64 	data[bufferSize];			// Array to hold data points from DAQ board.
float64		baselineTorque;				// Baseline torque value to zero out gain.
float 		maxVal = 4.0;				// Max value for gain calculations.
float 		minVal = -4.0;				// Min value for gain calculations.
int32		pointsToRead = 1;			// How many values should be read from the DAQ board in one call.
float64		timeout = 60.0;				// How long the DAQ board should wait for data before throwing an error.
int 		runMode = 0; 				// 1 = Movement/Active Mode, 0 = Still/Stop Mode
int 		displayMode = 1;			// 1 = verticalBar, 2 = sineGradient, 3 = horizontalBar
float 		visualPositionShift = 0;		// Initial value for how much the world is "shifted" (object movement).
float 		gain = 0.5;				// How much the pattern is shifted by.
float 		gainShift = 0.5;			// Added to gain (how much pattern is shifted) to increase speed w/up&down keys.
float 		speed = 1;				// Initial speed multiplier
float 		size = 1;				// Initial size multiplier.
int 		sizeMod = 1;				// For w(ide) and t(iny), this is how much it increases/decreases by.
int 		sizeOrig = 20;				// How big the patterns are originally.
float 		rawTorque = 0;				// Raw torque value (0-255)
int 		window_width = 480;			// Set initial window width.
int 		window_height = 640;			// Set initial window height.
float 		curve = 2;				// Curvature of the projection.
int 		bias = 0;				// Initial bias value.
int 		biasMod = 1;				// How much bias is shifted by.
int 		dataCounter = 0;			// For counting numberOfSamples.
int 		numberOfSamples = 6000;			// Number of data points taken and sent to MATLAB.
int 		recordMode = 0;				// 0 = No recording, 1 = Recording.
int 		oscillation = 0;			// Tracks how much the oscillating pattern has shifted.
int 		oscillationFlag = 0;			// Back and forth counter for oscillating pattern.
int 		oscillationMode = 0;			// Whether the program is in oscillation mode.
float 		minutes = 0;				// Tracking # of times the system time seconds have wrapped.
float 		previousTime = 0;			// Determines whether system time seconds have wrapped from 60->0.
int		openLoop = 1;				// Open Loop operation = 1, Closed Loop operation = 0.

// MATLAB Functionality.
Engine 		*ep;					// Initializes MATLAB engine.
mxArray 	*visualSamples;				// MATLAB array for visual position of object.
mxArray 	*timingSamples;				// MATLAB array for timing information.
mxArray 	*torqueSamples;				// MATLAB array for torque readings.
mxArray		*configData;				// MATLAB array for configuration data.
double 		*dataSetVisual;				// Pointer for visual information.
double 		*dataSetTiming;				// Pointer for timing information.
double 		*dataSetTorque;				// Pointer for torque information.
double 		*dataSetConfig;				// Pointer for configuration information.

static GLubyte checkImage[checkImageHeight][checkImageWidth][4];	// Where the gradient pattern is stored.
static GLuint texName;							// Texture name.

void setupDAQ();						// Configure DAQ board and setup DAQ handler.
void readDAQ();							// Pull a reading from the DAQ board.

/* Oscillation mode (no torque closed loop input, used to measure raw torque response). */
void oscillationFn() {
	if (oscillationFlag == 1) {
		speed = fabs(127-(rawTorque+bias));			// Absolute value of how far away from the midline you are.
		speed = pow((speed/50),2);				// (how far from the midline you are / 50)^2
		visualPositionShift=visualPositionShift+(gain*speed);
		if (visualPositionShift >= (window_width-(sizeOrig*size))) {
			oscillationFlag = 0;
		}
	} else {
		speed = fabs(127-(rawTorque-bias));			// Absolute value of how far away from the midline you are.
		speed = pow((speed/50),2);				// (how far from the midline you are / 50)^2
		visualPositionShift=visualPositionShift-(gain*speed);
		if (visualPositionShift <= 0) {
			oscillationFlag = 1;
		}
	}
}

/* Wraps single objects around to the other side of the screen. */
void visualScreenWrap() {
	if (visualPositionShift > window_width+(sizeOrig*size)) {		
		visualPositionShift = -(sizeOrig*size);	
	} else if (visualPositionShift < -(sizeOrig*size)) { 
		visualPositionShift = (window_width+(sizeOrig*size));	
	}
}

/* Defines the shifting and speed of the visual image. */
void speedControl() {
	readDAQ();							// Pulls a rawTorque value from the DAQ function.	
	if (openLoop == 1) {
		rawTorque = 127.5;
	}
	if (oscillationMode == 1) {
		oscillationFn();
	} else {
		speed = fabs(127-(rawTorque+bias));			// Absolute value of how far away from the midline you are.
		speed = pow((speed/50),2);				// (how far from the midline you are / 50)^2
		if ((rawTorque+bias)<127) {
			visualPositionShift=visualPositionShift-(gain*speed);
		} else {
			visualPositionShift=visualPositionShift+(gain*speed);
		}
		visualScreenWrap();
	}
}

/* Gathers second and millisecond data for MATLAB processing. */
void print_time () {
	struct timeval tv;
	struct tm* ptm;
	char time_string[10];
	float secsAndMillis;
	float milliseconds = 0;
	
	gettimeofday(&tv, NULL);						// Obtain the time of day, and convert it to a tm struct.
	ptm = localtime(&tv.tv_sec);
	strftime(time_string, sizeof(time_string), "%S", ptm);			// Format the date and time, down to a single second.
	milliseconds = tv.tv_usec/1000;
	secsAndMillis = strtol(time_string,NULL,10) + (milliseconds/1000);	// Tag it on w/the milliseconds; tv.tv_usec/1000 = milliseconds / 1000 = .ms
	if (previousTime > secsAndMillis) {
		minutes++;							// If the seconds have wrapped, add a minute.
	}
	dataSetTiming[dataCounter] = ((minutes*60) + secsAndMillis);		// Save it for MATLAB w/minute value adding 60 for every minute elapsed.
	previousTime = secsAndMillis;	
	dataCounter++;
}

/* Record data for MATLAB processing. */
void recordData() { 
	if (dataCounter<numberOfSamples) {
		dataSetVisual[dataCounter] = visualPositionShift+((sizeOrig*size)/2);
		dataSetTorque[dataCounter] = data[0];
		print_time();
	} else if (dataCounter == numberOfSamples) {
		engPutVariable(ep, "torqueSamples", torqueSamples);
		engPutVariable(ep, "visualSamples", visualSamples); 
		engPutVariable(ep, "timingSamples", timingSamples);
		engPutVariable(ep, "configData", configData);	
		engEvalString(ep, "eval(['save VSL_data_' date '.mat'])");
		fprintf(stderr, "Recording of %i samples is complete.  Data saved as \"visual_torque_data.mat\".\n", numberOfSamples);
		recordMode = 0;
		dataCounter++;
	} 
}

/* Simple line movement controlled via serialport_read input; speed is controlled via shifter function. */
void horizontalBar() {
	glTranslatef(visualPositionShift,window_height/2,0);	// Shifts along x-axis in active torque mode.
		
	int corner = 0;
	int x2 = sizeOrig*size*4;
	int y2 = window_height/30;

	glBegin(GL_QUADS);
		glColor3f(1.0f, 0.0f, 0.0f);			// Green
		glVertex2f(corner,corner);
		glVertex2f(x2,corner);
		glVertex2f(x2,y2);
		glVertex2f(corner,y2);
	glEnd();
}

/* Rendering function for the sine gradient. */
void sineGradient() {
	glTranslatef(window_width/2,window_height,0);		// Shift along the x axis.
	glRotatef(90,1.0,0,0);					// Rotates the cylinder to face the screen.
	glRotatef(visualPositionShift,0,0,1.0);

	glColor3f(0.0f,1.0f,0.0f);				// Red
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
	glTranslatef(visualPositionShift, 0.0, 0.0);		// Shift along the x axis.
	
	int corner = 0;
	int x2 = sizeOrig*size;
	int y2 = window_height;
	
	glBegin(GL_QUADS);
		glColor3f(0.0f, 0.0f, 1.0f);				// Blue
		glVertex2f(corner,corner);
		glVertex2f(x2,corner);
		glVertex2f(x2,y2);
		glVertex2f(corner,y2);
	glEnd();
}

void solidCircle() {
	glTranslatef(visualPositionShift, window_height/2, 0.0);	// Shift along the x axis.

	
	int angle = 0;
	int k1 = 0;
	glBegin(GL_TRIANGLE_FAN);
		glColor3f(1.0f, 1.0f, 1.0f);				// Green
		glVertex2f(k1, k1);
		for (angle = 0; angle <= 360; angle++) {
			glVertex2f(k1 + sin(angle) * (sizeOrig*size), k1 + cos(angle) * (sizeOrig*size));
		}
	glEnd();
}

/* myKeyboardFunc():  Keyboard input menu.  */
void myKeyboardFunc( unsigned char key, int x, int y ) {
	switch ( key ) {
	case 'h':
		fprintf(stderr, "[b]: Prints the current torque.\n\\
[c]: Increases the curvature of the display (opposite of [p]).\n\\
[f]: Full screen mode.\n\\
[g]: Non-oscillatory torque mode (opposite of [o]; see also [s]).\n\\
[h]: Prints this help menu.\n\\
[k]: Resets the bias and gain values.\n\\
[l]: Closed loop mode (torque influences visual flow; opposite of [m]).\n\\
[m]: Open loop mode (torque does NOT influence visual flow; opposite of [l]).\n\\
[o]: Oscillatory torque mode (opposite of [g]).\n\\
[p]: Decrease curvature of the display (opposite of [c]).\n\\
[r]: Record data.\n\\
[s]: Stop all movement on screen.\n\\
[t]: Decrease the width of the display pattern (opposite of [w]).\n\\
[w]: Increase the width of the display pattern (opposite of [t]).\n\\
[0]: Blank screen (no display pattern).\n\\
[1]: Vertical bar display pattern.\n\\
[2]: Sine gradient display pattern.\n\\
[3]: Horizontal bar display pattern.\n\\
[4]: Circular display pattern.\n\\
[.]: Increasing clockwise bias (decreasing counterclockwise bias, [,]).\n\\
[,]: Increasing counterclockwise bias (decreasing clockwise bias, [.]).\n\\
[Up]: Increase gain.\n\\
[Down]: Decrease gain.\n\\
[Esc]: Exit full screen mode.\n"); 	
		break;
	case 'b':
		readDAQ();
		fprintf(stderr, "[b]:  Current torque value is:  %f.\n", rawTorque);
		break;
	case 'c':
		fprintf(stderr, "[c]:  Increasing the curvature of the display to: %.3f).\n", curve);
		curve = curve+0.5;
		glutPostRedisplay();
		break;
	case 'f':
		glutFullScreen();
		break;
	case 'g':
		fprintf(stderr, "[g]:  Active non-oscillatory torque mode.  Press \"s\" to stop.\n");
		runMode = 1;
		oscillationMode = 0;
		glutPostRedisplay();
		break;
	case 'k':
		fprintf(stderr, "[k]:  Resetting bias and gain.\n");
		gain = 0.6;
		bias = 0;
		break;
	case 'l':
		fprintf(stderr, "[l]:  Entering CLOSED LOOP mode -- torque influences visual flow.\n");
		openLoop = 0;
		break;
	case 'm':
		fprintf(stderr, "[m]:  Entering OPEN LOOP mode -- torque DOES NOT influence visual flow.\n");
		openLoop = 1;
		break;
	case 'o':
		fprintf(stderr, "[o]:  Active oscillatory torque mode.  Press \"s\" to stop.\n");
		runMode = 1;
		oscillationMode = 1;
		break;
	case 'p':
		fprintf(stderr, "[p]:  Decreasing the curvature of the display to: %.3f).\n.", curve);
		curve = curve-0.5;
		glutPostRedisplay();
		break;
	case 'q':
		DAQmxBaseStopTask (taskHandle);
	       	DAQmxBaseClearTask (taskHandle);
		exit(1);
	case 'r':
		fprintf(stderr, "[r]:  Recording %i samples.\n", numberOfSamples);
		fprintf(stderr, "Current displayMode is:  %i.\n", displayMode);
		fprintf(stderr, "Current size is:  %.3f.\n", size);
		fprintf(stderr, "Current curvature of the display is:  %.3f.\n", curve);
		fprintf(stderr, "Current bias is:  %i.\n", bias);
		fprintf(stderr, "Current gain is:  %.3f.\n\n", gain);
		fprintf(stderr, "Baseline torque is:  %.3f.\n", baselineTorque);
		fprintf(stderr, "maxVal is:  %.3f.\n", maxVal);
		fprintf(stderr, "minVal is:  %.3f.\n", minVal);
		dataSetConfig[0] = displayMode;
		dataSetConfig[1] = size;
		dataSetConfig[2] = curve;
		dataSetConfig[3] = bias;
		dataSetConfig[4] = gain;
		dataSetConfig[5] = baselineTorque;
		dataSetConfig[6] = maxVal;
		dataSetConfig[7] = minVal;
		recordMode = 1;
		dataCounter = 0;
		runMode = 1;
		glutPostRedisplay();
		break;
	case 's':
		fprintf(stderr, "[s]:  Stop mode.  Press \"g\" to run.\n");
		oscillationMode = 0;
		runMode = 0;
		break;
	case 't':
		fprintf(stderr, "[t]:  Decreasing width by %i, size is now: %.3f.\n", sizeMod, size);
		size = size-sizeMod;
		glutPostRedisplay();
		break;
	case 'w':
		fprintf(stderr, "[w]:  Increasing width by %i, size is now: %.3f.\n", sizeMod, size);
		size = size+sizeMod;
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
		visualPositionShift = 0;
		glutPostRedisplay();
		break;
	case '2':
		fprintf(stderr, "[2]:  Setting displayMode to 2 (sineGradient).\n");
		displayMode = 2;
		visualPositionShift = 0;
		glutPostRedisplay();
		break;
	case '3':
		fprintf(stderr, "[3]:  Setting displayMode to 3 (horizontalBar).\n");
		displayMode = 3;
		visualPositionShift = 0;
		glutPostRedisplay();
		break;
	case '4':
		fprintf(stderr, "[4]:  Setting displayMode to 4 (solidCircle).\n");
		displayMode = 4;
		visualPositionShift = 0;
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
        checkImage[i][j][3] = (GLubyte) 255;			// Alpha
      }
   }
}

int main(int argc, char **argv) {
	// Opens MATLAB engine, creates matrices & pointers for visual and timing samples.
	ep = engOpen(NULL);
	visualSamples = mxCreateDoubleMatrix(numberOfSamples, 1, mxREAL);
	timingSamples = mxCreateDoubleMatrix(numberOfSamples, 1, mxREAL);
	torqueSamples = mxCreateDoubleMatrix(numberOfSamples, 1, mxREAL);	
	configData = mxCreateDoubleMatrix(10,1,mxREAL);	
	dataSetVisual = mxGetPr(visualSamples);
	dataSetTiming = mxGetPr(timingSamples);
	dataSetTorque = mxGetPr(torqueSamples);	
	dataSetConfig = mxGetPr(configData);
	setupDAQ();								// Initializes the DAQ board.	
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(window_width,window_height);				// Window size (in pixels).
	glutInitWindowPosition(100,100);					// Window position (from upper left).
	glutCreateWindow("Visual Stimulation Laboratory");			// Window title.
	init();
	glutKeyboardFunc(myKeyboardFunc);					// Handles "normal" ascii symbols (letters).
	glutSpecialFunc(mySpecialKeyFunc);					// Handles "special" keyboard keys (up, down).
	glutDisplayFunc(display);						// Default display mode.
	glutIdleFunc(idleDisplay);						// Used with glutSwapBuffers();
	glEnable(GL_DEPTH_TEST);
	makeCheckImage();							// Generates the gradient pattern in the background.
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, checkImageWidth, checkImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, checkImage);
	glutMainLoop();
}

/* Performs an actual read operation from the DAQ board. */
void readDAQ() {
	DAQmxBaseReadAnalogF64(taskHandle,pointsToRead,timeout,0,data,bufferSize,&pointsRead,NULL);
	rawTorque = ((data[0]-minVal)/(maxVal-minVal))*255;
	//fprintf(stderr,"data[0] is %f, data[1] is %f, rawTorque is: %f\n",data[0],data[1],rawTorque);
	pointsRead = 0; 
}

/* Configures the DAQ board for recording, calculates the baseline value (voltage offset from zero). */
void setupDAQ() {
        char        	chan[] = "Dev1/ai0";
        float64     	min = -6.0;
        float64     	max = 6.0;
        //char        	source[] = "OnboardClock";
        uInt64          samplesPerChan = 1;
        //float64     	sampleRate = 600.0;
 
	DAQmxBaseCreateTask("",&taskHandle);
	DAQmxBaseCreateAIVoltageChan(taskHandle,chan,"",DAQmx_Val_RSE,min,max,DAQmx_Val_Volts,NULL);
	//DAQmxBaseCfgSampClkTiming(taskHandle,source,sampleRate,DAQmx_Val_Rising,DAQmx_Val_ContSamps,samplesPerChan);
	// Use implicit timing -- we're not recording at a specific data rate.	
	DAQmxBaseCfgImplicitTiming(taskHandle,DAQmx_Val_ContSamps,samplesPerChan);	
	DAQmxBaseCfgInputBuffer(taskHandle,0); 	
	DAQmxBaseStartTask(taskHandle);

	fprintf(stderr,"DAQ Setup Complete.\n");

	// Takes 5 values to calculate a baseline for data calculation.	
	for (int i = 0; i<5; i++) {
		readDAQ();
		baselineTorque = baselineTorque+data[0];
	}
	baselineTorque = baselineTorque/5;
	fprintf(stderr,"Baseline torque sensor value:  %f.\n",baselineTorque);
	
	maxVal = maxVal+baselineTorque;
	minVal = minVal+baselineTorque;

	fprintf(stderr,"maxVal:  %f, minVal:  %f.\n",maxVal,minVal);		
	fprintf(stderr,"Press h to display the [h]elp menu.\n");
}
