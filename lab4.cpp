#include <GL/glew.h>
#include <GL/freeglut.h>
#include <stdio.h>
//#include "MathHelper.h"
#include "Matrix.h"		
//#include "phlegm_small.h"	// NEW! - look at this file if you haven't!  
							// It has the vertices[], normals[] and indices[] in it

//#define USING_INDEX_BUFFER 1

#ifdef USING_INDEX_BUFFER
	#define NUM_VERTICES 8	// NEW!  Note, these values are in the .h file too
	#define NUM_INDICES  24	
#else
	#define NUM_VERTICES 36
#endif

// From http://www.opengl.org/registry/specs/EXT/pixel_buffer_object.txt 
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

GLfloat light[] = {-1.0f, 0.0f, 0.5f, 1.0f}; // NEW!

GLuint shaderProgramID;
GLuint vao = 0;
GLuint vbo;
GLuint positionID, normalID; // NEW
GLuint indexBufferID; 

GLuint	perspectiveMatrixID, viewMatrixID, modelMatrixID;	// IDs of variables mP, mV and mM in the shader
GLuint	allRotsMatrixID; //NEW
GLuint	lightID;		// NEW

GLfloat* rotXMatrix;	// Matrix for rotations about the X axis
GLfloat* rotYMatrix;	// Matrix for rotations about the Y axis
GLfloat* rotZMatrix;	// Matrix for rotations about the Z axis

GLfloat* allRotsMatrix;	// NEW - we keep all of the model's rotations in this matrix (for rotating normals)

GLfloat* transMatrix;	// Matrix for changing the position of the object
GLfloat* transMatrix2;
GLfloat* scaleMatrix;	// Duh..
GLfloat* tempMatrix1;	// A temporary matrix for holding intermediate multiplications
GLfloat* tempMatrix2;
GLfloat* M;				// The final model matrix M to change into world coordinates

GLfloat* V;				// The camera matrix (for position/rotation) to change into camera coordinates
GLfloat* P;				// The perspective matrix for the camera (to give the scene depth); initialize this ONLY ONCE!

GLfloat  theta;			// An amount of rotation along one axis
GLfloat  gama;
GLfloat	 scaleAmount;	// In case the object is too big or small


void initMatrices() {

	theta = 0.0f;
	gama = 0.0f;
	scaleAmount = 1.0f;

	// Allocate memory for the matrices and initialize them to the Identity matrix
	rotXMatrix = new GLfloat[16];	makeIdentity(rotXMatrix);
	rotYMatrix = new GLfloat[16];	makeIdentity(rotYMatrix);
	rotZMatrix = new GLfloat[16];	makeIdentity(rotZMatrix);
	allRotsMatrix = new GLfloat[16]; makeIdentity(allRotsMatrix);
	transMatrix = new GLfloat[16];	makeIdentity(transMatrix);
	transMatrix2 = new GLfloat[16];	makeIdentity(transMatrix);
	scaleMatrix = new GLfloat[16];	makeIdentity(scaleMatrix);
	tempMatrix1 = new GLfloat[16];	makeIdentity(tempMatrix1);
	tempMatrix2 = new GLfloat[16];	makeIdentity(tempMatrix1);
	
	M = new GLfloat[16];			makeIdentity(M);
	V = new GLfloat[16];			makeIdentity(V);
	P = new GLfloat[16];			makeIdentity(P);

	// Set up the (P)erspective matrix only once! Arguments are 1) the resulting matrix, 2) FoV, 3) aspect ratio, 4) near plane 5) far plane
	makePerspectiveMatrix(P, 40.0f, 1.0f, 1.0f, 1000.0f);
	//makeOrtho(P,-1.0f,1.0f,-1.0f,1.0f,1.0f,1000.0f);
}

#pragma region SHADER_FUNCTIONS
static char* readFile(const char* filename) {
	// Open the file
	FILE* fp = fopen (filename, "r");
	// Move the file pointer to the end of the file and determing the length
	fseek(fp, 0, SEEK_END);
	long file_length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char* contents = new char[file_length+1];
	// zero out memory
	for (int i = 0; i < file_length+1; i++) {
		contents[i] = 0;
	}
	// Here's the actual read
	fread (contents, 1, file_length, fp);
	// This is how you denote the end of a string in C
	contents[file_length+1] = '\0';
	fclose(fp);
	return contents;
}

bool compiledStatus(GLint shaderID){
	GLint compiled = 0;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compiled);
	if (compiled) {
		return true;
	}
	else {
		GLint logLength;
		glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
		char* msgBuffer = new char[logLength];
		glGetShaderInfoLog(shaderID, logLength, NULL, msgBuffer);
		printf ("%s\n", msgBuffer);
		delete (msgBuffer);
		return false;
	}
}

GLuint makeVertexShader(const char* shaderSource) {
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource (vertexShaderID, 1, (const GLchar**)&shaderSource, NULL);
	glCompileShader(vertexShaderID);
	bool compiledCorrectly = compiledStatus(vertexShaderID);
	if (compiledCorrectly) {
		return vertexShaderID;
	}
	return -1;
}

GLuint makeFragmentShader(const char* shaderSource) {
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderID, 1, (const GLchar**)&shaderSource, NULL);
	glCompileShader(fragmentShaderID);
	bool compiledCorrectly = compiledStatus(fragmentShaderID);
	if (compiledCorrectly) {
		return fragmentShaderID;
	}
	return -1;
}

GLuint makeShaderProgram (GLuint vertexShaderID, GLuint fragmentShaderID) {
	GLuint shaderID = glCreateProgram();
	glAttachShader(shaderID, vertexShaderID);
	glAttachShader(shaderID, fragmentShaderID);
	glLinkProgram(shaderID);
	return shaderID;
}
#pragma endregion SHADER_FUNCTIONS

// Any time the window is resized, this function gets called.  It's setup to the 
// "glutReshapeFunc" in main.
void changeViewport(int w, int h){
	glViewport(0, 0, w, h);
}

// Here is the function that gets called each time the window needs to be redrawn.
// It is the "paint" method for our program, and is set up from the glutDisplayFunc in main
void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgramID);
	//theta = 0.01f;
	scaleAmount = 0.8f; //sin(theta);

	// Set the (M)odel matrix
	makeScale(scaleMatrix, scaleAmount, scaleAmount, scaleAmount);	// Fill the scaleMatrix variable
	makeRotateY(rotYMatrix,0.5);						// Fill the rotYMatrix variable
	makeRotateX(rotXMatrix,0.0);
	makeTranslate(transMatrix, 0.0f, 0.0f, -2.0f);	// Fill the transMatrix to push the model back 1 "unit" into the scene
	makeTranslate(transMatrix2, 0.0f, 0.0f, 0.0f);

	// Multiply them together 
	matrixMult4x4(tempMatrix1,rotYMatrix, scaleMatrix);	// Scale, then rotate...
	matrixMult4x4(tempMatrix2,rotXMatrix,tempMatrix1);
	matrixMult4x4(M, transMatrix, tempMatrix2);				// ... then multiply THAT by the translate
	
	// NEW!  Copy the rotations into the allRotsMatrix
	copyMatrix(rotYMatrix, allRotsMatrix);

	// Set the (V)iew matrix if you want to "move" around the scene
     makeTranslate(V, theta, gama, 0.0f);	
	
	// Important! Pass that data to the shader variables
	glUniformMatrix4fv(modelMatrixID, 1, GL_TRUE, M);
	glUniformMatrix4fv(viewMatrixID, 1, GL_TRUE, V);
	glUniformMatrix4fv(perspectiveMatrixID, 1, GL_TRUE, P);
	glUniformMatrix4fv(allRotsMatrixID, 1, GL_TRUE, allRotsMatrix);
	glUniform4fv(lightID, 1, light);
	
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#ifdef USING_INDEX_BUFFER
	glDrawElements (GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_INT, NULL);
#else
	glDrawArrays(GL_TRIANGLES, 0, NUM_VERTICES);
#endif
	glutSwapBuffers();
	glutPostRedisplay();		// This calls "render" again, allowing for animation!
}

void keyboard(unsigned char key,int x,int y){
	switch(key){
		case'S':
		theta-=0.01f; break;
		case'D':
		theta+=0.01f; break;
		case'W':
		gama += 0.01f; break;
		case'A':
		gama -=0.01f; break;

	}
}


int main (int argc, char** argv) {
	// Standard stuff...
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
	glutInitWindowSize(800, 600);
	glutCreateWindow("Diffuse Lighting");
	glutReshapeFunc(changeViewport);
	glutDisplayFunc(render);
	glewInit();

	initMatrices(); 

	GLfloat vertices[]= {
	-0.5f,0.5f,0.5f,  -0.5f,-0.5f,0.5f,  0.5f,-0.5f,0.5f,  -0.5f,0.5f,0.5f,  0.5f,-0.5f,0.5f,   0.5f,0.5f, 0.5f,
	 0.5f,0.5f,0.5f,   0.5f,-0.5f,0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,
	 0.5f,-0.5f,0.5f, -0.5f,-0.5f,0.5f, -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f, -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
	 0.5f,0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f,0.5f,0.5f,   0.5f,0.5f,-0.5f,  -0.5f,0.5f,0.5f,   0.5f,0.5f,0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f,0.5f,-0.5f,  0.5f,0.5f,-0.5f,  -0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
	-0.5f,0.5f,-0.5f,  -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f, -0.5f,0.5f,-0.5f, -0.5f,-0.5f,0.5f,  -0.5f,0.5f,0.5f,


	};

	//GLuint indices[] = {1,0,3,2,2,3,7,6,3,0,4,7,6,5,1,2,4,5,6,7,5,4,0,1};

	GLfloat normals[]={
	0.0f,0.0f,1.0f,  0.0f,0.0f,1.0f,  0.0f,0.0f,1.0f,  0.0f,0.0f,1.0f,  0.0f,0.0f,1.0f,  0.0f,0.0f,1.0f,
	1.0f,0.0f,0.0f,  1.0f,0.0f,0.0f,  1.0f,0.0f,0.0f,  1.0f,0.0f,0.0f,  1.0f,0.0f,0.0f,  1.0f,0.0f,0.0f,
	0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f, 0.0f,-1.0f,0.0f,
	0.0f,1.0f,0.0f,  0.0f,1.0f,0.0f,  0.0f,1.0f,0.0f,  0.0f,1.0f,0.0f,  0.0f,1.0f,0.0f,  0.0f,1.0f,0.0f,
	0.0f,0.0f,-1.0f, 0.0f,0.0f,-1.0f, 0.0f,0.0f,-1.0f, 0.0f,0.0f,-1.0f,  0.0f,0.0f,-1.0f, 0.0f,0.0f,-1.0f,
	-1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f, -1.0f,0.0f,0.0f,
};



	/*GLfloat vertices[] = {0.0f, 0.5f, 0.0f, // 0
						-0.25f, 0.0f, 0.0f, // 1
						0.25f, 0.0f, 0.0f, // 2
						-0.5f, -0.5f, 0.0f, // 3
						0.0f, -0.5f, 0.0f, // 4
						0.5f, -0.5f, 0.0f // 5
	};

	GLuint indices[] = {0, 1, 2, 1, 3, 4, 2, 4, 5};


	GLfloat normals[]={
	0.0f,0.0f,1.0f,
	0.0f,0.0f,1.0f,
	0.0f,0.0f,1.0f,
	0.0f,0.0f,1.0f,
	0.0f,0.0f,1.0f,
	0.0f,0.0f,1.0f
};*/


	// Make a shader
	char* vertexShaderSourceCode = readFile("vertexShader.vsh");
	char* fragmentShaderSourceCode = readFile("fragmentShader.fsh");
	GLuint vertShaderID = makeVertexShader(vertexShaderSourceCode);
	GLuint fragShaderID = makeFragmentShader(fragmentShaderSourceCode);
	shaderProgramID = makeShaderProgram(vertShaderID, fragShaderID);

	// Create the "remember all"
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	// Create the buffer, but don't load anything yet
	//glBufferData(GL_ARRAY_BUFFER, 7*NUM_VERTICES*sizeof(GLfloat), NULL, GL_STATIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, 6*NUM_VERTICES*sizeof(GLfloat), NULL, GL_STATIC_DRAW);		// NEW!! - We're only loading vertices and normals (6 elements, not 7)
	
	// Load the vertex points
	glBufferSubData(GL_ARRAY_BUFFER, 0, 3*NUM_VERTICES*sizeof(GLfloat), vertices);
	// Load the colors right after that
	//glBufferSubData(GL_ARRAY_BUFFER, 3*NUM_VERTICES*sizeof(GLfloat),4*NUM_VERTICES*sizeof(GLfloat), colors);
	glBufferSubData(GL_ARRAY_BUFFER, 3*NUM_VERTICES*sizeof(GLfloat),3*NUM_VERTICES*sizeof(GLfloat), normals);
	
	
#ifdef USING_INDEX_BUFFER
	glGenBuffers(1, &indexBufferID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, NUM_INDICES*sizeof(GLuint), indices, GL_STATIC_DRAW);
#endif
	
	// Find the position of the variables in the shader
	positionID = glGetAttribLocation(shaderProgramID, "s_vPosition");
	normalID = glGetAttribLocation(shaderProgramID, "s_vNormal");
	lightID = glGetUniformLocation(shaderProgramID, "vLight");	// NEW
	
	// ============ glUniformLocation is how you pull IDs for uniform variables===============
	perspectiveMatrixID = glGetUniformLocation(shaderProgramID, "mP");
	viewMatrixID = glGetUniformLocation(shaderProgramID, "mV");
	modelMatrixID = glGetUniformLocation(shaderProgramID, "mM");
	allRotsMatrixID = glGetUniformLocation(shaderProgramID, "mRotations");	// NEW
	//=============================================================================================

	glVertexAttribPointer(positionID, 3, GL_FLOAT, GL_FALSE, 0, 0);
	//glVertexAttribPointer(colorID, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(vertices)));
	glVertexAttribPointer(normalID, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(vertices)));
	
	glUseProgram(shaderProgramID);
	glEnableVertexAttribArray(positionID);
	glEnableVertexAttribArray(normalID);
	
	
	glEnable(GL_CULL_FACE);  // NEW! - we're doing real 3D now...  Cull (don't render) the backsides of triangles
	glCullFace(GL_BACK);	// Other options?  GL_FRONT and GL_FRONT_AND_BACK
	glEnable(GL_DEPTH_TEST);// Make sure the depth buffer is on.  As you draw a pixel, update the screen only if it's closer than previous ones
	 glClearColor( 1.0, 1.0, 1.0, 1.0 );

	glutKeyboardFunc( keyboard );
	glutMainLoop();
	
	return 0;
}