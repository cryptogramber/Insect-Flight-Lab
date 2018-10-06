/* Torquometer Visual Tools, Amber Fechko, amber@kairozu.com
 *
 * Yes, this code is messy.  Amber:Programmer::"Someone that can make a grilled cheese":Chef  Suggestions welcomed.
 *
 * V2.1  Switched back to this after realizing couldn't use some of the usb fns with the host side.
 * V2.2  Turned on vsync in OS X, fixed a lot of flickering issues.
 * V3.0  Started using a rotating cylinder to display the gradient (for use against a curved surface).
 * V3.1  Fixed some of the serial buffer settings, added the invert colors feature.  
 * V4.0W WINDOWS HISSSS!!!!!!!!!!!!! */

#include "mex.h"
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>			// Needed for movement speed calculations.
#include <getopt.h>
#define checkImageWidth 128	// Width of openGL created visual gradient.
#define checkImageHeight 8	// Height of openGL created visual gradient.

int RunMode = 0; 			// 1 = Start Oscillating Continuously, 0 = Start in Still Mode
int displayMode = 1;		// 1 = lineRendering, 2 = coneRendering
float shift = 0;			// Initial value for total "shifted" amount (accumulates).
float mod = 0.6;			// How much the pattern is shifted by.
float size = 1;				// Initial size multiplier.
float sizeMod = 0.1;		// For w(ide) and t(iny), this is how much it increases/decreases by.
float sizeOrig = 40;		// How big the patterns are originally.
float modShift = 0.2;		// Added to mod (how much pattern is shifted) to increase speed w/up&down keys.
int x0 = 0;
int window_width = 480;		// Set initial window width.
int window_height = 640;	// Set initial window height.
float curve = 2;			// Curvature of the projection.
float colorTrue = 0.0;
float colorInvert = 1.0;
float colorHolder = 0.0;
float bias = 0.0;
float biasMod = 1.0;

static GLubyte checkImage[checkImageHeight][checkImageWidth][4];
static GLuint texName;

/* Simple line movement controlled via serialport_read input; speed is controlled via shifter function. */
void horizRendering() {
	glTranslatef(shift+window_width/4,window_height/2,0);		// shifts along x-axis
	
	int x1 = 0;
	int y1 = 0;
	int x2 = (window_width/3)+(sizeOrig+(sizeOrig*size));
	int y2 = window_height/8;

	glBegin(GL_QUADS);
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex2f(x1,y1);
		glVertex2f(x2,y1);
		glVertex2f(x2,y2);
		glVertex2f(x1,y2);
	glEnd();
	glFlush();
}

/* Rendering function for the sin gradient. */
void coneRendering() {
	glTranslatef(window_width/2,window_height,0);		// Shift along the x axis.

	glRotatef(90,1.0,0,0);
	glRotatef(shift/4,0,0,1.0);

	GLUquadric *myQuad;
	myQuad=gluNewQuadric();
	glEnable(GL_TEXTURE_2D);
	gluQuadricTexture(myQuad, GL_TRUE);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(10.0*size,1.0,1.0);
	//gluCylinder(quadric, base radius, top radius, height, slices, stacks);
	gluCylinder(myQuad, window_width/curve, window_width/curve, window_height, 256, 2);
	//glBindTexture(GL_TEXTURE_2D, texName);
	glFlush();
	glDisable(GL_TEXTURE_2D);
}

/* Simple line movement controlled via serialport_read input; speed is controlled via shifter function. */
void lineRendering() {
	glTranslatef(shift+window_width/2, 0.0, 0.0);					// Shift along the x axis.

	int x1 = 0;
	int y1 = 0;
	int x2 = x1+(sizeOrig+(sizeOrig*size));
	int y2 = window_height;

	glBegin(GL_QUADS);
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex2f(x1,y1);
		glVertex2f(x2,y1);
		glVertex2f(x2,y2);
		glVertex2f(x1,y2);
	glEnd();
	glFlush();

	if (shift > (window_width/2)+(sizeOrig+(sizeOrig*size))) {
		shift = -((window_width/2)+((sizeOrig+(sizeOrig*size))/2));
	} else if (shift < -(window_width/2)-(sizeOrig+(sizeOrig*size))) {
		shift = ((window_width/2)+((sizeOrig+(sizeOrig*size))/2));
	}
}

/* myKeyboardFunc():  Keyboard input menu.  */
void myKeyboardFunc( unsigned char key, int x, int y ) {
	switch ( key ) {
	case 'f':
		glutFullScreen();
		break;
	case 'g':
		fprintf(stderr, "[g]:  Active torque mode.  Press \"s\" to stop.\n");
		fprintf(stderr, "Baseline reading is:  %i\n", x0);
		RunMode = 1;
		glutPostRedisplay();
		break;
	case 's':
		fprintf(stderr, "[s]:  Stop mode.  Press \"g\" to run.\n");
		RunMode = 0;
		break;
	case 'w':
		fprintf(stderr, "[w]:  Increasing width by %f.  Press \"t\" to decrease.\n", sizeMod);
		size = size+sizeMod;
		glutPostRedisplay();
		break;
	case 't':
		fprintf(stderr, "[t]:  Decreasing width by %f.  Press \"w\" to increase.\n", sizeMod);
		size = size-sizeMod;
		glutPostRedisplay();
		break;
	case 'c':
		fprintf(stderr, "[c]:  Increasing the curvature of the display (now: %f).  Press \"p\" to decrease.\n", curve);
		curve = curve+0.5;
		break;
	case 'p':
		fprintf(stderr, "[p]:  Decreasing the curvature of the display (now: %f).  Press \"c\" to increase.\n.", curve);
		curve = curve-0.5;
		break;
	case 'i':
		fprintf(stderr, "[i]:  Color invert.\n");
		colorHolder = colorTrue;
		colorTrue = colorInvert;
		colorInvert = colorHolder;
		glutPostRedisplay();
		break;
	case '1':
		fprintf(stderr, "[1]:  Setting displayMode to 1 (line shifting).\n");
		displayMode = 1;
		shift = 0;
	    //glutIdleFunc(lineRendering);
	    glutPostRedisplay();
		break;
	case '2':
		fprintf(stderr, "[2]:  Setting displayMode to 2 (sin(x) gradient).\n");
		displayMode = 2;
		shift = 0;
		//glutIdleFunc(coneRendering);
		glutPostRedisplay();
		break;
	case '3':
		fprintf(stderr, "[3]:  Setting displayMode to 3 (line shifting : horizontal).\n");
		displayMode = 3;
		shift = 0;
		//glutIdleFunc(horizRendering);
		glutPostRedisplay();
		break;
	case '.':
		bias=bias+biasMod;
		fprintf(stderr, "[.]:  Increasing clockwise bias.  Current bias is now: %f.\n",bias);
		break;
	case ',':
		bias=bias-biasMod;
		fprintf(stderr, "[,]:  Increasing counterclockwise bias.  Current bias is now:  %f.\n",bias);
		break;
	case 'h':
		fprintf(stderr,"Torquometer Visual Tools V3.0\n\n \
			Keyboard Menu:\n\t \
			[f]ull screen:  Enters full screen mode.\n\t \
			[g]o mode:  Real time torque sensor input display. \n\t \
			[s]top mode:  Stops displaying real time input. \n\t \
			[w]ide:  Grating pattern wider. \n\t \
			[t]hin:  Grating pattern thinner. \n\t \
			[c]urve:  Increase curvature. \n\t \
			[p]ress:  Decrease curvature. \n\t \
			[i]nvert:  Swap colors. \n\t \
			[up]:  Increases speed/response to torque input. \n\t \
			[down]:  Decreases speed/response to torque input. \n\t \
			[.]:  Increases clockwise bias. \n\t \
			[,]:  Increases counterclockwise bias. \n\t \
			[h]elp:  Print the help menu. \n\t \
			[q]uit:  Exit. \n\t \
			[esc]:  Exits full screen mode.\n");
		break;
	case 'q':
		exit(1);
	case 27:
		glutReshapeWindow(window_width,window_height);
		glutPositionWindow(100,100);
	}
}

/* mySpecialKeyFunc() = Keyboard input menu for special keys.
 * [up]:  Increases speed/response to torque input.
 * [down]:  Decreases speed/response to torque input. */
void mySpecialKeyFunc( int key, int x, int y ) {
	switch ( key ) {
	case GLUT_KEY_UP:
		if (mod<10) {				// Avoid overflow problems.
			mod=mod+modShift;
		}
		break;
	case GLUT_KEY_DOWN:
		if (mod>0.001) {			// Avoid underflow problems.
			mod=mod-modShift;
		}
		break;
	}
}

/* Defines the shifting and speed of the visual image. */
void speedControl() {
	int x1 = 0;

	glClearColor(colorTrue, colorTrue, colorTrue, 0.0);
	//serialport_read(fd, serialbuffer);				// Grabs the next byte from usbserial.

	float speed=fabs(125-x0);						// Absolute value of how far away from the midline you are.
	speed = pow((speed/50),2);						// (how far from the midline you are / 50)^2
	//fprintf(stderr, "speed is %f\n",speed);

	//float x1=x0+bias;
	//fprintf(stderr, "x1 is %f\n",x1);

	x1=x0+bias;

	if (RunMode == 1) {
		if (x1<125) {
			shift=shift-(mod*speed);
		} else {
			shift=shift+(mod*speed);
		}
	}
}

/* Initialization function -- runs once to define the visual field. */
void init() {
	/* Changing the GL_PROJECTION matrix alters how the world is seen for the viewer, though it does not change the
	 * positions of any of the objects in the world including the view position.  */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,window_width,window_height,0,-window_width,window_width);
}

/* Main glutDisplayLoop; controls the higher level GL functions between the rendering functions. */
void display(void) {
	speedControl();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (displayMode == 1) {
		lineRendering();
	} else if (displayMode == 2) {
		coneRendering();
	} else if (displayMode == 3) {
		horizRendering();
	}

	glPopMatrix();
	glFlush();
	glutSwapBuffers();

	if (RunMode==1) {
		glutPostRedisplay();					// Marks window for destruction/redraw.
	}
}

void changeSize(int w, int h) {
	fprintf(stderr, "width is %i\n",w);
	fprintf(stderr, "height is %i\n",h);

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	/*if(h == 0)
		h = 1;

	float ratio = 1.0* w / h;

	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Set the viewport to be the entire window
	glViewport(0, 0, w, h);



	// Set the correct perspective.
	gluPerspective(45.0f,ratio,1.0f,1000.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0,0.0,5.0,
			  0.0,0.0,-1.0,
			  0.0f,1.0f,0.0f); */
}


/* Creates the actual texture map for use with coneRendering().  checkImage[] holds pixels w/RGBA values.
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

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
//int main(int argc, char **argv) 
    glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(window_width,window_height);					// Window size (in pixels).
	glutInitWindowPosition(100,100);								// Window position (from upper left).
	glutCreateWindow("Torquometer Visual Field");					// Window title.
	init();
	glutKeyboardFunc(myKeyboardFunc);								// Handles "normal" ascii symbols (letters).
	glutSpecialFunc(mySpecialKeyFunc);								// Handles "special" keyboard keys (up, down).
	glutDisplayFunc(display);										// Default display mode.
	//glutIdleFunc(coneRendering);									// Used with glutSwapBuffers();
	//glutIdleFunc(display);
	glutReshapeFunc(changeSize);
	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_TEXTURE_2D);
	makeCheckImage();												// Generates the gradient pattern in the background.
	//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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
