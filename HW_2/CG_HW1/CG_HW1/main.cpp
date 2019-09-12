#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <freeglut/glut.h>
#include "textfile.h"
#include "glm.h"

#include "Matrices.h"

#define PI 3.1415926

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
GLuint iLocR;
GLuint iLocLightIdx;
GLuint iLocKa;
GLuint iLocKd;
GLuint iLocKs;
GLuint iLocShininess;

struct iLocLightInfo
{
	GLuint position;
	GLuint ambient;
	GLuint diffuse;
	GLuint specular;
	GLuint spotDirection;
	GLuint spotCutoff;
	GLuint spotExponent;
	GLuint constantAttenuation;
	GLuint linearAttenuation;
	GLuint quadraticAttenuation;
}iLocLightInfo[3];

struct LightInfo
{
	Vector4 position;
	Vector4 spotDirection;
	Vector4 ambient;
	Vector4 diffuse;
	Vector4 specular;
	float spotExponent;
	float spotCutoff;
	float constantAttenuation;
	float linearAttenuation;
	float quadraticAttenuation;
}lightInfo[3];

struct Group
{
	int numTriangles;
	GLfloat *vertices;
	GLfloat *normals;
	GLfloat ambient[4];
	GLfloat diffuse[4];
	GLfloat specular[4];
	GLfloat shininess;
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

int current_x, current_y;

Model* models;	// store the models we load
vector<string> filenames; // .obj filename list
int cur_idx = 0; // represent which model should be rendered now
int color_mode = 0;
bool use_wire_mode = false;
float rotateVal = 0.0f;
int timeInterval = 33;	// miliseconds
bool isRotate = true;
bool enableAmbient = true;
bool enableDiffuse = true;
bool enableSpecular = true;
float scaleOffset = 0.65f;

int light_idx = 0;

struct Frustum {
	float left, right, top, bottom, cnear, cfar;
};

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

Matrix4 myOrthogonal(Frustum frustum)
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
	m.numGroups = m.obj->numgroups;
	m.group = new Group[m.numGroups];
	GLMgroup* group = m.obj->groups;

	GLfloat maxVal[3] = {-100000, -100000, -100000 };
	GLfloat minVal[3] = { 100000, 100000, 100000 };

	int curGroupIdx = 0;

	// Iterate all the groups of this model
	while (group)
	{
		m.group[curGroupIdx].vertices = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].normals = new GLfloat[group->numtriangles * 9];
		m.group[curGroupIdx].numTriangles = group->numtriangles;

		// Fetch material information
		memcpy(m.group[curGroupIdx].ambient, m.obj->materials[group->material].ambient, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].diffuse, m.obj->materials[group->material].diffuse, sizeof(GLfloat) * 4);
		memcpy(m.group[curGroupIdx].specular, m.obj->materials[group->material].specular, sizeof(GLfloat) * 4);

		m.group[curGroupIdx].shininess = m.obj->materials[group->material].shininess;

		// For each triangle in this group
		for (int i = 0; i < group->numtriangles; i++)
		{
			int triangleIdx = group->triangles[i];
			int indv[3] = {
				m.obj->triangles[triangleIdx].vindices[0],
				m.obj->triangles[triangleIdx].vindices[1],
				m.obj->triangles[triangleIdx].vindices[2]
			};
			int indn[3] = {
				m.obj->triangles[triangleIdx].nindices[0],
				m.obj->triangles[triangleIdx].nindices[1],
				m.obj->triangles[triangleIdx].nindices[2]
			};

			// For each vertex in this triangle
			for (int j = 0; j < 3; j++)
			{
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 0] = m.obj->vertices[indv[j] * 3 + 0];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 1] = m.obj->vertices[indv[j] * 3 + 1];
				m.group[curGroupIdx].vertices[i * 9 + j * 3 + 2] = m.obj->vertices[indv[j] * 3 + 2];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 0] = m.obj->normals[indn[j] * 3 + 0];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 1] = m.obj->normals[indn[j] * 3 + 1];
				m.group[curGroupIdx].normals[i * 9 + j * 3 + 2] = m.obj->normals[indn[j] * 3 + 2];

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
		traverseColorModel(models[idx++]);
	}
}

void onIdle()
{
	glutPostRedisplay();
}

void drawModel(Model &m)
{
	int groupNum = m.numGroups;

	glBindVertexArray(vaoHandle);
	glUniform1i(iLocLightIdx, light_idx);

	for (int i = 0; i < groupNum; i++)
	{
		Matrix4 N = m.N;

		glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles*9, m.group[i].vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, normalBufferHandle);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*m.group[i].numTriangles * 9, m.group[i].normals, GL_DYNAMIC_DRAW);

		glUniform4fv(iLocKa, 1, m.group[i].ambient);
		glUniform4fv(iLocKd, 1, m.group[i].diffuse);
		glUniform4fv(iLocKs, 1, m.group[i].specular);
		glUniform1f(iLocShininess, m.group[i].shininess);

		glUniformMatrix4fv(iLocP, 1, GL_FALSE, P.getTranspose());
		glUniformMatrix4fv(iLocV, 1, GL_FALSE, V.getTranspose());
		glUniformMatrix4fv(iLocN, 1, GL_FALSE, N.getTranspose());
		glUniformMatrix4fv(iLocR, 1, GL_FALSE, R.getTranspose());

		glDrawArrays(GL_TRIANGLES, 0, m.group[i].numTriangles * 3);
	}
}

void drawTriangle()
{
	glBindVertexArray(vaoHandle);

	float fakeColor[] = {
		0.5f, 0.8f, 0.3f, 1.0f
	};

	Matrix4 MVP = P*V;
	glUniformMatrix4fv(iLocN, 1, GL_FALSE, MVP.getTranspose());
	glUniform4fv(iLocLightInfo[0].position, 1, fakeColor);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void timerFunc(int timer_value)
{
	if (isRotate)
	{
		rotateVal += 1.0f;
	}

	R = RotateY(rotateVal);

	glutPostRedisplay();
	glutTimerFunc(timeInterval, timerFunc, 0);
}

void updateLight()
{
	if (enableAmbient)
	{
		glUniform4f(iLocLightInfo[0].ambient, lightInfo[0].ambient.x, lightInfo[0].ambient.y, lightInfo[0].ambient.z, lightInfo[0].ambient.w);
		glUniform4f(iLocLightInfo[1].ambient, lightInfo[1].ambient.x, lightInfo[1].ambient.y, lightInfo[1].ambient.z, lightInfo[1].ambient.w);
		glUniform4f(iLocLightInfo[2].ambient, lightInfo[2].ambient.x, lightInfo[2].ambient.y, lightInfo[2].ambient.z, lightInfo[2].ambient.w);
	}
	else
	{
		float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glUniform4fv(iLocLightInfo[0].ambient, 1, zeros);
		glUniform4fv(iLocLightInfo[1].ambient, 1, zeros);
		glUniform4fv(iLocLightInfo[2].ambient, 1, zeros);
	}

	if (enableDiffuse)
	{
		glUniform4f(iLocLightInfo[0].diffuse, lightInfo[0].diffuse.x, lightInfo[0].diffuse.y, lightInfo[0].diffuse.z, lightInfo[0].diffuse.w);
		glUniform4f(iLocLightInfo[1].diffuse, lightInfo[1].diffuse.x, lightInfo[1].diffuse.y, lightInfo[1].diffuse.z, lightInfo[1].diffuse.w);
		glUniform4f(iLocLightInfo[2].diffuse, lightInfo[2].diffuse.x, lightInfo[2].diffuse.y, lightInfo[2].diffuse.z, lightInfo[2].diffuse.w);
	}
	else
	{
		float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glUniform4fv(iLocLightInfo[0].diffuse, 1, zeros);
		glUniform4fv(iLocLightInfo[1].diffuse, 1, zeros);
		glUniform4fv(iLocLightInfo[2].diffuse, 1, zeros);
	}

	if (enableSpecular)
	{
		glUniform4f(iLocLightInfo[0].specular, lightInfo[0].specular.x, lightInfo[0].specular.y, lightInfo[0].specular.z, lightInfo[0].specular.w);
		glUniform4f(iLocLightInfo[1].specular, lightInfo[1].specular.x, lightInfo[1].specular.y, lightInfo[1].specular.z, lightInfo[1].specular.w);
		glUniform4f(iLocLightInfo[2].specular, lightInfo[2].specular.x, lightInfo[2].specular.y, lightInfo[2].specular.z, lightInfo[2].specular.w);
	}
	else
	{
		float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glUniform4fv(iLocLightInfo[0].specular, 1, zeros);
		glUniform4fv(iLocLightInfo[1].specular, 1, zeros);
		glUniform4fv(iLocLightInfo[2].specular, 1, zeros);
	}


	glUniform4f(iLocLightInfo[0].position, lightInfo[0].position.x, lightInfo[0].position.y, lightInfo[0].position.z, lightInfo[0].position.w);

	glUniform4f(iLocLightInfo[1].position, lightInfo[1].position.x, lightInfo[1].position.y, lightInfo[1].position.z, lightInfo[1].position.w);
	glUniform1f(iLocLightInfo[1].constantAttenuation, lightInfo[1].constantAttenuation);
	glUniform1f(iLocLightInfo[1].linearAttenuation, lightInfo[1].linearAttenuation);
	glUniform1f(iLocLightInfo[1].quadraticAttenuation, lightInfo[1].quadraticAttenuation);

	glUniform4f(iLocLightInfo[2].position, lightInfo[2].position.x, lightInfo[2].position.y, lightInfo[2].position.z, lightInfo[2].position.w);
	glUniform4f(iLocLightInfo[2].spotDirection, lightInfo[2].spotDirection.x, lightInfo[2].spotDirection.y, lightInfo[2].spotDirection.z, lightInfo[2].spotDirection.w);
	glUniform1f(iLocLightInfo[2].spotExponent, lightInfo[2].spotExponent);
	glUniform1f(iLocLightInfo[2].spotCutoff, lightInfo[2].spotCutoff);
	glUniform1f(iLocLightInfo[2].constantAttenuation, lightInfo[2].constantAttenuation);
	glUniform1f(iLocLightInfo[2].linearAttenuation, lightInfo[2].linearAttenuation);
	glUniform1f(iLocLightInfo[2].quadraticAttenuation, lightInfo[2].quadraticAttenuation);
}

void onDisplay(void)
{
	// clear canvas
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// clear canvas to color(0,0,0)->black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	updateLight();

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
	glEnableVertexAttribArray(1);	// Vertex color

	// Map index 0 to the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferHandle);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	// Map index 1 to the color buffer
	glBindBuffer(GL_ARRAY_BUFFER, normalBufferHandle);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
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
	iLocR = glGetUniformLocation(p, "um4r");
	iLocLightIdx = glGetUniformLocation(p, "lightIdx");
	iLocKa = glGetUniformLocation(p, "material.Ka");
	iLocKd = glGetUniformLocation(p, "material.Kd");
	iLocKs = glGetUniformLocation(p, "material.Ks");
	iLocShininess = glGetUniformLocation(p, "material.shininess");

	iLocLightInfo[0].position = glGetUniformLocation(p, "light[0].position");
	iLocLightInfo[0].ambient = glGetUniformLocation(p, "light[0].La");
	iLocLightInfo[0].diffuse = glGetUniformLocation(p, "light[0].Ld");
	iLocLightInfo[0].specular = glGetUniformLocation(p, "light[0].Ls");
	iLocLightInfo[0].spotDirection = glGetUniformLocation(p, "light[0].spotDirection");
	iLocLightInfo[0].spotCutoff = glGetUniformLocation(p, "light[0].spotCutoff");
	iLocLightInfo[0].spotExponent = glGetUniformLocation(p, "light[0].spotExponent");
	iLocLightInfo[0].constantAttenuation = glGetUniformLocation(p, "light[0].constantAttenuation");
	iLocLightInfo[0].linearAttenuation = glGetUniformLocation(p, "light[0].linearAttenuation");
	iLocLightInfo[0].quadraticAttenuation = glGetUniformLocation(p, "light[0].quadraticAttenuation");
	

	iLocLightInfo[1].position = glGetUniformLocation(p, "light[1].position");
	iLocLightInfo[1].ambient = glGetUniformLocation(p, "light[1].La");
	iLocLightInfo[1].diffuse = glGetUniformLocation(p, "light[1].Ld");
	iLocLightInfo[1].specular = glGetUniformLocation(p, "light[1].Ls");
	iLocLightInfo[1].spotDirection = glGetUniformLocation(p, "light[1].spotDirection");
	iLocLightInfo[1].spotCutoff = glGetUniformLocation(p, "light[1].spotCutoff");
	iLocLightInfo[1].spotExponent = glGetUniformLocation(p, "light[1].spotExponent");
	iLocLightInfo[1].constantAttenuation = glGetUniformLocation(p, "light[1].constantAttenuation");
	iLocLightInfo[1].linearAttenuation = glGetUniformLocation(p, "light[1].linearAttenuation");
	iLocLightInfo[1].quadraticAttenuation = glGetUniformLocation(p, "light[1].quadraticAttenuation");

	iLocLightInfo[2].position = glGetUniformLocation(p, "light[2].position");
	iLocLightInfo[2].ambient = glGetUniformLocation(p, "light[2].La");
	iLocLightInfo[2].diffuse = glGetUniformLocation(p, "light[2].Ld");
	iLocLightInfo[2].specular = glGetUniformLocation(p, "light[2].Ls");
	iLocLightInfo[2].spotDirection = glGetUniformLocation(p, "light[2].spotDirection");
	iLocLightInfo[2].spotCutoff = glGetUniformLocation(p, "light[2].spotCutoff");
	iLocLightInfo[2].spotExponent = glGetUniformLocation(p, "light[2].spotExponent");
	iLocLightInfo[2].constantAttenuation = glGetUniformLocation(p, "light[2].constantAttenuation");
	iLocLightInfo[2].linearAttenuation = glGetUniformLocation(p, "light[2].linearAttenuation");
	iLocLightInfo[2].quadraticAttenuation = glGetUniformLocation(p, "light[2].quadraticAttenuation");
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

void onMouse(int who, int state, int x, int y)
{
	printf("%18s(): (%d, %d) ", __FUNCTION__, x, y);

	switch (who)
	{
		case GLUT_LEFT_BUTTON:
			if (light_idx == 2)
			{
				lightInfo[2].spotExponent *= 0.8;
			}
			current_x = x;
			current_y = y;
			break;
		case GLUT_MIDDLE_BUTTON: printf("middle button "); break;
		case GLUT_RIGHT_BUTTON:
			current_x = x;
			current_y = y;
			if (light_idx == 2)
			{
				lightInfo[2].spotExponent *= 1.2;
			}
			break;
		case GLUT_WHEEL_UP:
			printf("wheel up      \n");
			if (light_idx == 2)
			{
				lightInfo[2].spotCutoff += 0.1;
			}
			break;
		case GLUT_WHEEL_DOWN:
			printf("wheel down    \n");
			if (light_idx == 2)
			{
				lightInfo[2].spotCutoff -= 0.1;
			}
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
	lightInfo[0].position.x = lightInfo[1].position.x = (x - 400) / 50.0f;
	lightInfo[0].position.y = lightInfo[1].position.y  = -(y - 400) / 50.0f;

	lightInfo[2].position.x = (x - 400) / 400.0f;
	lightInfo[2].position.y = -(y - 400) / 400.0f;

	printf("%18s(): (%d, %d) mouse move\n", __FUNCTION__, x, y);
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
		light_idx = 0;
		break;
	case 'w':
		light_idx = 1;
		break;
	case 'e':
		light_idx = 2;
		break;
	case 'a':
		enableAmbient = !enableAmbient;
		break;
	case 's':
		enableSpecular = !enableSpecular;
		break;
	case 'd':
		enableDiffuse = !enableDiffuse;
		break;
	case 'c':
		lightInfo[1].position[2] += 0.1f;
		break;
	case 'v':
		lightInfo[1].position[2] -= 0.1f;
		break;
	case 'i':
		break;
	case 't':
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
		lightInfo[1].position[0] -= 0.1f;
		break;

	case GLUT_KEY_RIGHT:
		printf("key: RIGHT ARROW");
		lightInfo[1].position[0] += 0.1f;
		break;
	case GLUT_KEY_UP:
		printf("key: Up ARROW");
		lightInfo[1].position[1] += 0.1f;
		break;
	case GLUT_KEY_DOWN:
		printf("key: Down ARROW");
		lightInfo[1].position[1] -= 0.1f;
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
	Frustum frustum;
	Vector3 eyePosition = Vector3(0.0f, 0.0f, 2.0f);
	Vector3 centerPosition = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 upVector = Vector3(0.0f, 1.0f, 0.0f);

	frustum.left = -1.0f;	frustum.right = 1.0f;
	frustum.top = 1.0f;	frustum.bottom = -1.0f;
	frustum.cnear = 0.1f;	frustum.cfar = 10.0f;

	V = myViewingMatrix(eyePosition, centerPosition, upVector);
	P = myOrthogonal(frustum);

	lightInfo[0].position = Vector4(3.0f , 3.0f, 3.0f, 0.0f);
	lightInfo[0].ambient = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
	lightInfo[0].diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[0].specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	lightInfo[1].position = Vector4(0.0f, -2.0f, 1.0f, 1.0f);
	lightInfo[1].ambient = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
	lightInfo[1].diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[1].specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[1].constantAttenuation = 0.05;
	lightInfo[1].linearAttenuation = 0.3;
	lightInfo[1].quadraticAttenuation = 0.6f;

	lightInfo[2].position = Vector4(0.0f, 0.0f, 5.0f, 1.0f);
	lightInfo[2].ambient = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
	lightInfo[2].diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[2].specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	lightInfo[2].spotDirection = Vector4(0.0f, 0.0f, -1.0f, 0.0f);
	lightInfo[2].spotExponent = 5.0;
	lightInfo[2].spotCutoff = 3;
	lightInfo[2].constantAttenuation = 0.05;
	lightInfo[2].linearAttenuation = 0.3;
	lightInfo[2].quadraticAttenuation = 0.6f;
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
	glutInitWindowSize(800, 800);
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

