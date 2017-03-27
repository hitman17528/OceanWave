#include "pch.h"

int g_iWidth, g_iHeight;

float* g_pDisplayMap;
float* g_pObstruction;
float* g_pSource;

float* g_pHeight;
float* g_pPrevHeight;
float* g_pVerticalDerivative;

float g_fScalingFactor;
float g_mKernel[13][13];

int g_iPaintMode;
enum {PAINT_OBSTRUCTION, PAINT_SOURCE};

bool g_bRegenerateData;
bool g_bToggleAnimationOnOff;

float dt, alpha, gravity;

float obstructionBrush[3][3];
float sourceBrush[3][3];

int xMousePrev, yMousePrev;

int size;

// Initialize all of field to 0
void Initialize(float* data, float value)
{
	for (int i = 0; i < size; i++)
		data[i] = value;
}

// Compute elements of convolution kernel
void InitializeKernel()
{
	double dk = 0.01;
	double sigma = 1.0;
	double norm = 0.0;

	for (double k = 0.0; k < 10.0; k += dk)
	{
		norm += k * k * exp(-sigma * k * k);
	}

	for (int i = -6; i <= 6; i++)
	{
		for (int j = -6; j <= 6; j++)
		{
			double r = sqrt((float)(i * i + j * j));
			double kern = 0.0;
			for (double k = 0.0; k < 10.0; k += dk)
			{
				kern += k * k * exp(-sigma * k * k) * _j0(r * k);
			}

			g_mKernel[i + 6][j + 6] = kern / norm;
		}
	}
}

void InitializeBrushes()
{
	obstructionBrush[1][1] = 0.0f;
	obstructionBrush[1][0] = 0.5f;
	obstructionBrush[0][1] = 0.5f;
	obstructionBrush[2][1] = 0.5f;
	obstructionBrush[1][2] = 0.5f;
	obstructionBrush[0][2] = 0.75f;
	obstructionBrush[2][0] = 0.75f;
	obstructionBrush[0][0] = 0.75f;
	obstructionBrush[2][2] = 0.75f;

	sourceBrush[1][1] = 1.0f;
	sourceBrush[1][0] = 0.5f;
	sourceBrush[0][1] = 0.5f;
	sourceBrush[2][1] = 0.5f;
	sourceBrush[1][2] = 0.5f;
	sourceBrush[0][2] = 0.25f;
	sourceBrush[2][0] = 0.25f;
	sourceBrush[0][0] = 0.25f;
	sourceBrush[2][2] = 0.25f;
}

void ClearObstruction()
{
	for (int i = 0; i < size; i++)
		g_pObstruction[i] = 1.0f;
}

void ClearWaves()
{
	for (int i = 0; i < size; i++)
	{
		g_pHeight[i] = 0.0f;
		g_pPrevHeight[i] = 0.0f;
		g_pVerticalDerivative[i] = 0.0f;
	}
}

void ConvertToDisplay()
{
	for (int i = 0; i < size; i++)
	{
		g_pDisplayMap[i] = 0.5f * (g_pHeight[i] / g_fScalingFactor + 1.0f) * g_pObstruction[i];
	}
}

//
// These two routines,
//
// ComputeVerticalDerivative()
// Propagate()
//
// are the heart of the iWave algorithm.
//
// In Propagate(), we have not bothered to handle the
// boundary conditions. This makes the outermost
// 6 pixels all the way around act like a hard wall.
void ComputeVerticalDerivative()
{
	// first step: the interior
	for (int ix = 6; ix < g_iWidth - 6; ix++)
	{
		for (int iy = 6; iy < g_iHeight - 6; iy++)
		{
			int index = ix + g_iWidth * iy;
			float vd = 0.0f;
			for (int iix = -6; iix <= 6; iix++)
			{
				for (int iiy = -6; iiy <= 6; iiy++)
				{
					int iindex = ix + iix + g_iWidth * (iy + iiy);
					vd += g_mKernel[iix + 6][iiy + 6] * g_pHeight[iindex];
				}
			}

			g_pVerticalDerivative[index] = vd;
		}
	}
}

void Propagate()
{
	// apply obstruction
	for (int i = 0; i < size; i++)
	{
		g_pHeight[i] *= g_pObstruction[i];
	}

	ComputeVerticalDerivative();

	// advance surface
	float adt = alpha * dt;
	float adt2 = 1.0f / (1.0f + adt);
	for (int i = 0; i < size; i++)
	{
		float temp = g_pHeight[i];
		g_pHeight[i] = g_pHeight[i] * (2.0f - adt) - g_pPrevHeight[i] - gravity * g_pVerticalDerivative[i];
		g_pHeight[i] *= adt2;
		g_pHeight[i] += g_pSource[i];
		g_pHeight[i] *= g_pObstruction[i];
		g_pPrevHeight[i] = temp;
		// reset source each step
		g_pSource[i] = 0;
	}
}

//------------------------------------------
//
// Painting and display code
//

void ResetScaleFactor(float amount)
{
	g_fScalingFactor *= amount;
}

void DabSomePaint(int x, int y)
{
	int xStart = x - 1;
	int yStart = y - 1;
	if (xStart < 0)	{ xStart = 0; }
	if (yStart < 0)	{ yStart = 0; }

	int xEnd = x + 1;
	int yEnd = y + 1;
	if (xEnd >= g_iWidth)	{ xEnd = g_iWidth - 1; }
	if (yEnd >= g_iHeight)	{ yEnd = g_iHeight - 1; }

	if (g_iPaintMode == PAINT_OBSTRUCTION)
	{
		for (int ix = xStart; ix <= xEnd; ix++)
		{
			for (int iy = yStart; iy <= yEnd; iy++)
			{
				int index = ix + g_iWidth * (g_iHeight - iy - 1);
				g_pObstruction[index] *= obstructionBrush[ix - xStart][iy - yStart];
			}
		}
	}
	else if (g_iPaintMode == PAINT_SOURCE)
	{
		for (int ix = xStart; ix <= xEnd; ix++)
		{
			for (int iy = yStart; iy <= yEnd; iy++)
			{
				int index = ix + g_iWidth * (g_iHeight - iy - 1);
				g_pSource[index] += sourceBrush[ix - xStart][iy - yStart];
			}
		}
	}
}

void Display()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawPixels(g_iWidth, g_iHeight, GL_LUMINANCE, GL_FLOAT, g_pDisplayMap);
	glutSwapBuffers();
}

// animate and display new result
void Idle()
{
	if (g_bToggleAnimationOnOff)
	{
		Propagate();
	}

	ConvertToDisplay();
	Display();
}

void OnKeyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case '-': 
	case '_':
		ResetScaleFactor(1.0 / 0.9);
		g_bRegenerateData = true;
		break;
	case '+': 
	case '=':
		ResetScaleFactor(0.9);
		g_bRegenerateData = true;
		break;
	case ' ' :
		g_bToggleAnimationOnOff = !g_bToggleAnimationOnOff;
	case 'o':
		g_iPaintMode = PAINT_OBSTRUCTION;
		break;
	case 's':
		g_iPaintMode = PAINT_SOURCE;
		break;
	case 'b':
		ClearWaves();
		ClearObstruction();
		Initialize(g_pSource, 0.0f);
		break;
	default:
		break;
	}
}

void OnMouseDown(int button, int state, int x, int y)
{
	if (button != GLUT_LEFT_BUTTON)
		return;

	if (state != GLUT_DOWN)
		return;

	xMousePrev = x;
	yMousePrev = y;
	DabSomePaint(x, y);
}

void OnMouseMove(int x, int y)
{
	xMousePrev = x;
	yMousePrev = y;
	DabSomePaint(x, y);
}

int main(int argc, char** argv)
{
	// initialize a few variables
	g_iWidth = g_iHeight = 400;
	size = g_iWidth * g_iHeight;

	// Init physical variables
	dt = 0.03f;
	alpha = 0.3f;
	gravity = 9.8f * dt * dt;

	g_fScalingFactor = 1.0f;
	g_bToggleAnimationOnOff = true;

	// allocate space for fields and initialize them
	g_pHeight = new float[size];
	g_pPrevHeight = new float[size];
	g_pVerticalDerivative = new float[size];
	g_pObstruction = new float[size];
	g_pSource = new float[size];
	g_pDisplayMap = new float[size];

	// Prepare for rendering
	ClearWaves();
	ClearObstruction();
	ConvertToDisplay();
	Initialize(g_pSource, 0.0f);

	// Initialize brush
	InitializeBrushes();

	// build convolution kernel
	InitializeKernel();

	// GLUT routines for rendering
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(g_iWidth, g_iHeight);
	int windowID = glutCreateWindow("GL iWave demo");

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	glutDisplayFunc(Display);
	glutIdleFunc(Idle);
	glutKeyboardFunc(OnKeyboard);
	glutMouseFunc(OnMouseDown);
	glutMotionFunc(OnMouseMove);

	glutMainLoop();
	return 0;
}