#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <freeglut/glut.h>
#include "textfile.h"
#include "glm.h"
#include "Texture.h"

#include "Matrices.h"

#define PI 3.1415926
#define MAX_TEXTURE_NUM 50

#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "freeglut.lib")

#ifndef GLUT_WHEEL_UP
# define GLUT_WHEEL_UP   0x0003
# define GLUT_WHEEL_DOWN 0x0004
#endif

#ifndef GLUT_KEY_ESC
# define GLUT_KEY_ESC 0x001B
#endif

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Shader attributes
GLuint vaoHandle;
GLuint vboHandles[2];
GLuint positionBufferHandle;
GLuint normalBufferHandle;

// Shader attributes for uniform variables
GLuint iLocP;
GLuint iLocV;
GLuint iLocN;
GLuint iLocMVP;
GLuint iLocTex;

enum Mode
{
	Eye,
	Transform,
	Light
};
Mode mode;

struct Group
{
	int numTriangles;
	GLfloat *vertices;
	GLfloat *texcoords;
	FileHeader fileHeader;
	InfoHeader infoHeader;
	unsigned char *image;
	GLuint texNum;

};

struct Model
{
	int numGroups;
	GLMmodel *obj;
	Group *group;
	Matrix4 N;
	Vector3 position = Vector3(0,0,0);
	Vector3 scale = Vector3(1,1,1);
	Vector3 rotation = Vector3(0,0,0);	// Euler form
};

Matrix4 V = Matrix4(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1);

Matrix4 P = Matrix4(
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, -1, 0,
	0, 0, 0, 1);

Matrix4 R;

int height = 800, width = 800;
int last_x, last_y;

Model* models;	// store the models we load
vector<string> filenames; // .obj filename list
int cur_idx = 0; // represent which model should be rendered now
bool use_wire_mode = false;
float rotateVal = 0.0f;
int timeInterval = 33;
bool isRotate = true;
float scaleOffset = 0.65f;

int light_idx = 0;

bool mag_linear = true;
bool min_linear = true;
bool wrap_clamp = true;

void LoadTextures(string filename, Model& model, int index)
{
	unsigned long size;
	FILE *f = fopen(filename.c_str(), "rb");
	fread(&model.group[index].fileHeader, sizeof(FileHeader), 1, f);
	fread(&model.group[index].infoHeader, sizeof(InfoHeader), 1, f);
	size = model.group[index].infoHeader.Width * model.group[index].infoHeader.Height * 3;
	model.group[index].image = new unsigned char[size];
	fread(&model.group[index].image, size * sizeof(unsigned char), 1, f);
	fclose(f);
}

struct Frustum {
	float left, right, top, bottom, cnear, cfar;
};

struct Camera {
	Vector3 position = Vector3(0.0f,0.0f,2.0f);
	Vector3 center = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 upVector = Vector3(0.0f, 1.0f, 0.0f);
};

Frustum frustum;
Camera camera;
float moveCoef = 0.05f;

Matrix4 translate(Vector3 vec)
{
	Matrix4 mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 myViewingMatrix(Vector3 cameraPosition, Vector3 cameraViewDirection, Vector3 cameraUpVector)
{
	Vector3 X, Y, Z;
	Z = (cameraPosition - cameraViewDirection).normalize();
	Y = cameraUpVector.normalize();
	X = Y.cross(Z).normalize();
	cameraUpVector = Z.cross(X);

	Matrix4 mat = Matrix4(
		X.x, X.y, X.z, -X.dot(cameraPosition),
		Y.x, Y.y, Y.z, -Y.dot(cameraPosition),
		Z.x, Z.y, Z.z, -Z.dot(cameraPosition),
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 perspective(Frustum frustum)
{
	GLfloat n = frustum.cnear;
	GLfloat f = frustum.cfar;
	GLfloat r = frustum.right;
	GLfloat l = frustum.left;
	GLfloat t = frustum.top;
	GLfloat b = frustum.bottom;

	Matrix4 mat = Matrix4(
		2*n/(r-l), 0, (r+l)/(r-l), 0,
		0, 2*n/(t-b), (t+b)/(t-b), 0,
		0, 0, -(f+n)/(f-n), -(2*f*n)/(f-n),
		0, 0, -1, 0
	);

	return mat;
}

Matrix4 orthogonal(Frustum frustum)
{
	GLfloat tx = -(frustum.right + frustum.left) / (frustum.right - frustum.left);
	GLfloat ty = -(frustum.top + frustum.bottom) / (frustum.top - frustum.bottom);
	GLfloat tz = -(frustum.cfar + frustum.cnear) / (frustum.cfar - frustum.cnear);
	Matrix4 mat = Matrix4(
		2 / (frustum.right - frustum.left), 0, 0, tx,
		0, 2 / (frustum.top - frustum.bottom), 0, ty,
		0, 0, -2 / (frustum.cfar - frustum.cnear), tz,
		0, 0, 0, 1
	);

	return mat;
}

Matrix4 RotateY(float val)
{
	val = PI*val / 180.0;

	Matrix4 mat;
	mat[0] = cos(val);
	mat[1] = 0;
	mat[2] = sin(val);
	mat[3] = 0;

	mat[4] = 0;
	mat[5] = 1;
	mat[6] = 0;
	mat[7] = 0;

	mat[8] = -sin(val);
	mat[9] = 0;
	mat[10] = cos(val);
	mat[11] = 0;

	mat[12] = 0;
	mat[13] = 0;
	mat[14] = 0;
	mat[15] = 1;

	return mat;
}

void traverseColorModel(Model &m)
{
	int tex_cnt = 0;

	m.numGroups = m.obj->numgroups;
	m.group = new Group[m.numGroups];
	GLMgroup* group = m.obj->groups;

	GLfloat maxVal[3] = {-100000, -100000, -100000 };
	GLfloat minVal[3] = { 100000, 100000, 100000 };

	int curGroupIdx = 0;

	// Iterate all the groups of this model
	while (group)
	{
		// If there exist material in this group
		if (strlen(m.obj->materials[group->material].textureImageName) != 0)
		{
			string texName = "../../../TextureModels/" + string(m.obj->materials[group->material].textureImageName);

			cout << texName << endl;
			
			unsigned long size;
			FILE *f = fopen(texName.c_str(), "rb");
			fread(&m.group[curGroupIdx].fileHeader, sizeof(FileHeader), 1, f);
			fread(&m.group[curGroupIdx].infoHeader, sizeof(InfoHeader), 1, f);
			size = m.group[curGroupIdx].infoHeader.Width * m.group[curGroupIdx].infoHeader.Height * 3;
			m.group[curGroupIdx].image = new unsigned char[size];
			fread(m.group[curGroupIdx].image, size * sizeof(char), 1, f);
			fclose(f);

			//[TODO] fill in the blank
			glGenTextures(1, &m.group[curGroupIdx].texNum);
			glBindTexture(GL_TEXTURE_2D, m.group[curGroupIdx].texNum);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m.group[curGroupIdx].infoHeader.Width, m.group[curGroupIdx].infoHeader.Height, 0, GL_BGR, GL_UNSIGNED_BYTE, m.group[curGroupIdx].image);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		m.group[curGroupIdx].vertices = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].texcoords = new GLfloat[group->numtriangles * 6];
		m.group[curGroupIdx].numTriangles = group->numtriangles;

		// For each triangle in this group
		for (int i = 0; i < group->numtriangles; i++)
		{
			int triangleIdx = group->triangles[i];
			int indv[3] = {
				m.obj->triangles[triangleIdx].vindices[0],
				m.obj->triangles[triangleIdx].vindices[1],
				m.obj->triangles[triangleIdx].vindices[2]
			};
			int indt[3] = {
				m.obj->triangles[triangleIdx].tindices[0],
				m.obj->triangles[triangleIdx].tindices[1],
				m.obj->triangles[triangleIdx].tindices[2]
			};

			// For each vertex in this triangle
			for (int j = 0; j < 3; j++)
			{
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0] = m.obj->vertices[indv[j] * 3 + 0];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1] = m.obj->vertices[indv[j] * 3 + 1];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2] = m.obj->vertices[indv[j] * 3 + 2];
				m.group[curGroupIdx].texcoords[i * 6 + j * 2 + 0] = m.obj->texcoords[indt[j] * 2 + 0];
				m.group[curGroupIdx].texcoords[i * 6 + j * 2 + 1] = m.obj->texcoords[indt[j] * 2 + 1];

				maxVal[0] = max(maxVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				maxVal[1] = max(maxVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				maxVal[2] = max(maxVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);

				minVal[0] = min(minVal[0], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0]);
				minVal[1] = min(minVal[1], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1]);
				minVal[2] = min(minVal[2], m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2]);
			}
		}
		group = group->next;
		curGroupIdx++;
	}

	// Normalize the model
	float norm_scale = max(max(abs(maxVal[0] - minVal[0]), abs(maxVal[1] - minVal[1])), abs(maxVal[2] - minVal[2]));
	Matrix4 S, T;

	S[0] = 2 / norm_scale*scaleOffset;	S[5] = 2 / norm_scale*scaleOffset;	S[10] = 2 / norm_scale*scaleOffset;
	T[3] = -(maxVal[0] + minVal[0]) / 2;
	T[7] = -(maxVal[1] + minVal[1]) / 2;
	T[11] = -(maxVal[2] + minVal[2]) / 2;
	m.N = S*T;
}

void loadOBJModel()
{
	models = new Model[filenames.size()];
	int idx = 0;
	for (string filename : filenames)
	{
		models[idx].obj = glmReadOBJ((char*)filename.c_str());
		traverseColorModel(models[idx]);
		idx++;
	}
}

void onIdle()
{
	glutPostRedisplay();
}

// [TODO] fill in the blank
void handleTexParameter()
{
	if(mag_linear)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if(min_linear)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	if (wrap_clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void drawModel(Model &m)
{
	int groupNum = m.numGroups;

	glBindVertexArray(vaoHandle);

	for (int i = 0; i < groupNum; i++)
	{
		Matrix4 MVP = P * V * R*m.N;

		glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles*9, m.group[i].vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, normalBufferHandle);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 6, m.group[i].texcoords, GL_DYNAMIC_DRAW);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m.group[i].texNum);

		glUniformMatrix4fv(iLocMVP, 1, GL_FALSE, MVP.getTranspose());

		glDrawArrays(GL_TRIANGLES, 0, m.group[i].numTriangles * 3);
	}
}

void timerFunc(int timer_value)
{
	if (isRotate)
	{
		rotateVal += 2.0f;
	}

	R = RotateY(rotateVal);

	glutPostRedisplay();
	glutTimerFunc(timeInterval, timerFunc, 0);
}

void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawModel(models[cur_idx]);

	glutSwapBuffers();
}

void showShaderCompileStatus(GLuint shader, GLint *shaderCompiled)
{
	glGetShaderiv(shader, GL_COMPILE_STATUS, shaderCompiled);
	if (GL_FALSE == (*shaderCompiled))
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character.
		GLchar *errorLog = (GLchar*)malloc(sizeof(GLchar) * maxLength);
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
		fprintf(stderr, "%s", errorLog);

		glDeleteShader(shader);
		free(errorLog);
	}
}

void setVertexArrayObject()
{
	// Create and setup the vertex array object
	glGenVertexArrays(1, &vaoHandle);
	glBindVertexArray(vaoHandle);

	// Enable the vertex attribute arrays
	glEnableVertexAttribArray(0);	// Vertex position
	glEnableVertexAttribArray(1);	// Vertex uv

	// Map index 0 to the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Map index 1 to the color buffer
	glBindBuffer(GL_ARRAY_BUFFER, normalBufferHandle);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void setVertexBufferObjects()
{
	// Triangles Data
	float positionData[] = {
		-0.8f, -0.8f, 0.0f,
		0.8f, -0.8f, 0.0f,
		0.0f, 0.8f, 0.0f };

	float colorData[] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f };

	glGenBuffers(2, vboHandles);

	positionBufferHandle = vboHandles[0];
	normalBufferHandle = vboHandles[1];

	// Populate the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9, positionData, GL_STATIC_DRAW);

	// Populate the color buffer
	glBindBuffer(GL_ARRAY_BUFFER, normalBufferHandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9, colorData, GL_STATIC_DRAW);
}

void setUniformVariables(GLuint p)
{
	iLocP = glGetUniformLocation(p, "um4p");
	iLocV = glGetUniformLocation(p, "um4v");
	iLocN = glGetUniformLocation(p, "um4n");
	iLocMVP = glGetUniformLocation(p, "um4mvp");
	iLocTex = glGetUniformLocation(p, "tex");
	glUniform1i(iLocTex, 0);
}

void setShaders()
{
	GLuint v, f, p;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vert");
	fs = textFileRead("shader.frag");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	// compile vertex shader
	glCompileShader(v);
	GLint vShaderCompiled;
	showShaderCompileStatus(v, &vShaderCompiled);
	if (!vShaderCompiled) system("pause"), exit(123);

	// compile fragment shader
	glCompileShader(f);
	GLint fShaderCompiled;
	showShaderCompileStatus(f, &fShaderCompiled);
	if (!fShaderCompiled) system("pause"), exit(456);

	p = glCreateProgram();

	// bind shader
	glAttachShader(p, f);
	glAttachShader(p, v);

	// link program
	glLinkProgram(p);
	glUseProgram(p);

	setUniformVariables(p);
	setVertexBufferObjects();
	setVertexArrayObject();
}

void moveCameraForward()
{
	Vector3 forward = (camera.center - camera.position).normalize();
	camera.position += moveCoef*forward;
	camera.center += moveCoef*forward;
	V = myViewingMatrix(camera.position, camera.center, camera.upVector);
}

void moveCameraBackward()
{
	Vector3 forward = (camera.center - camera.position).normalize();
	camera.position -= moveCoef*forward;
	camera.center -= moveCoef*forward;
	V = myViewingMatrix(camera.position, camera.center, camera.upVector);
}

void moveCameraLeft()
{
	Vector3 forward = (camera.center - camera.position).normalize();
	Vector3 up = (camera.upVector).normalize();
	Vector3 right = forward.cross(up);
	camera.position -= moveCoef*right;
	camera.center -= moveCoef*right;
	V = myViewingMatrix(camera.position, camera.center, camera.upVector);
}

void moveCameraRight()
{
	Vector3 forward = (camera.center - camera.position).normalize();
	Vector3 up = (camera.upVector).normalize();
	Vector3 right = forward.cross(up);
	camera.position += moveCoef*right;
	camera.center += moveCoef*right;
	V = myViewingMatrix(camera.position, camera.center, camera.upVector);
}

float deg2radian(float degree)
{
	return degree*PI / 180.0f;
}

float radian2deg(float radian)
{
	return radian*180.0f / PI;
}

void onMouse(int who, int state, int x, int y)
{
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);

	switch (who)
	{
		case GLUT_LEFT_BUTTON:
			last_x = x;
			last_y = y;
			break;
		case GLUT_MIDDLE_BUTTON: printf("middle button "); break;
		case GLUT_RIGHT_BUTTON:
			break;
		case GLUT_WHEEL_UP:
			printf("wheel up      \n");
			break;
		case GLUT_WHEEL_DOWN:
			printf("wheel down    \n");
			break;
		default:                 
			printf("0x%02X          ", who); break;
	}

	switch (state)
	{
		case GLUT_DOWN: printf("start "); break;
		case GLUT_UP:   printf("end   "); break;
	}

	printf("\n");
}

void onMouseMotion(int x, int y)
{
	printf("%18s(): (%d, %d) mouse move\n", __FUNCTION__, x, y);
	last_x = x;
	last_y = y;
}

void onKeyboard(unsigned char key, int x, int y)
{
	printf("%18s(): (%d, %d) key: %c(0x%02X) ", __FUNCTION__, x, y, key, key);
	switch (key)
	{
		case GLUT_KEY_ESC: /* the Esc key */
			exit(0);
			break;
		case 'z':
			cur_idx = (cur_idx + filenames.size() - 1) % filenames.size();
			break;
		case 'x':
			cur_idx = (cur_idx + 1) % filenames.size();
			break;
		case 'q':
			break;
		case 'w':
			break;
		case 'e':
			break;
		case 'a':
			break;
		case 's':
			break;
		case 'd':
			break;
		case 'o':
			P = orthogonal(frustum);
			break;
		case 'p':
			P = perspective(frustum);
			break;
		case 'f':
			mag_linear = !mag_linear;
			handleTexParameter();
			break;
		case 'g':
			min_linear = !min_linear;
			handleTexParameter();
			break;
		case 'h':
			wrap_clamp = !wrap_clamp;
			handleTexParameter();
			break;
		case 'r':
			isRotate = !isRotate;
			break;
	}
	printf("\n");
}

void onKeyboardSpecial(int key, int x, int y) {
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);
	switch (key)
	{
	case GLUT_KEY_LEFT:
		printf("key: LEFT ARROW");
		break;

	case GLUT_KEY_RIGHT:
		printf("key: RIGHT ARROW");
		break;

	default:
		printf("key: 0x%02X      ", key);
		break;
	}
	printf("\n");
}

void onWindowReshape(int width, int height)
{
	printf("%18s(): %dx%d\n", __FUNCTION__, width, height);
}

void initParameter()
{
	Vector3 eyePosition = Vector3(0.0f, 0.0f, 3.0f);
	Vector3 centerPosition = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 upVector = Vector3(0.0f, 1.0f, 0.0f);

	frustum.left = -1.0f;	frustum.right = 1.0f;
	frustum.top = 1.0f;	frustum.bottom = -1.0f;
	frustum.cnear = 0.1f;	frustum.cfar = 100.0f;

	V = myViewingMatrix(eyePosition, centerPosition, upVector);
	P = orthogonal(frustum);
}

void loadConfigFile()
{
	ifstream fin;
	string line;
	fin.open("../../config.txt", ios::in);
	if (fin.is_open())
	{
		while (getline(fin, line))
		{
			filenames.push_back(line);
		}
		fin.close();
	}
	else
	{
		cout << "Unable to open the config file!" << endl;
	}
	for (int i = 0; i < filenames.size(); i++)
		printf("%s\n", filenames[i].c_str());
}

int main(int argc, char **argv)
{
	loadConfigFile();

	// glut init
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);

	// create window
	glutInitWindowPosition(500, 100);
	glutInitWindowSize(height, width);
	glutCreateWindow("CG HW1");

	glewInit();
	if (glewIsSupported("GL_VERSION_4_3")) {
		printf("Ready for OpenGL 4.3\n");
	}
	else {
		printf("OpenGL 4.3 not supported\n");
		system("pause");
		exit(1);
	}

	initParameter();
	handleTexParameter();

	// load obj models through glm
	loadOBJModel();

	// register glut callback functions
	glutDisplayFunc(onDisplay);
	glutIdleFunc(onIdle);
	glutKeyboardFunc(onKeyboard);
	glutSpecialFunc(onKeyboardSpecial);
	glutMouseFunc(onMouse);
	glutMotionFunc(onMouseMotion);
	glutReshapeFunc(onWindowReshape);
	glutTimerFunc(timeInterval, timerFunc, 0);

	// set up shaders here
	setShaders();

	glEnable(GL_DEPTH_TEST);

	// main loop
	glutMainLoop();

	// delete glm objects before exit
	for (int i = 0; i < filenames.size(); i++)
	{
		glmDelete(models[i].obj);
	}

	return 0;
}

