#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<glm/common.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include<GL/gl.h>

#include<iostream>
#include<chrono>
#include<thread>
#include<string>
#include<vector>
#include<tuple>
#define _USE_MATH_DEFINES
#include<math.h>
#include<random>

#define GLEE() {  \
	GLenum err=glGetError();  \
	 if(err!=GL_NO_ERROR) {  \
		std::cerr<<__FILE__<<":"<<__LINE__<<": 0x"<<std::hex<<err<<": "<<gluErrorString(err)<<std::endl;  \
		abort();  \
	 }  \
}

//#define GLEE() {}

const std::string TESSELATION_VS=R"glsl(
	#version 400

	layout(location = 0) in vec4 in_position;
	layout(location = 1) in vec3 in_color;

	out vec3 vs_color;

	void main()
	{
		gl_Position=in_position;
		vs_color=in_color;
	}

)glsl";

const std::string TESSELATION_TCS=R"glsl(
	#version 400

	layout(vertices=1) out;

	in vec3 vs_color[];
	patch out vec3 tcs_color;			// patch <=> one value for whole patch, not for each vertex

	void main()
	{
		gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
		gl_TessLevelOuter[1]=gl_TessLevelOuter[3]=gl_TessLevelInner[1]=10;
		gl_TessLevelOuter[0]=gl_TessLevelOuter[2]=gl_TessLevelInner[0]=gl_TessLevelOuter[1]*2;

		tcs_color=vs_color[0];						// color of first vertex is taken as color of sphere
	}

)glsl";

const std::string TESSELATION_TES=R"glsl(
	#version 400

	layout(quads, equal_spacing) in;

	patch in vec3 tcs_color;
	out vec3 tes_color;

	uniform mat4 MVP;

	#define M_PI 3.1415926535897932384626433832795

	void main(){
		float psi = gl_TessCoord.x*2*M_PI;
		float phi = gl_TessCoord.y*M_PI;
		float r = gl_in[0].gl_Position.w;
		float dy = cos(phi)*r;
		r = max(sqrt(r*r-dy*dy), 0);				// due float inaccuracy can sqrt result be nan
		float dx = sin(psi)*r;
		float dz = cos(psi)*r;
		vec3 pt = gl_in[0].gl_Position.xyz+vec3(dx,dy,dz);
		gl_Position=MVP*vec4(normalize(pt), 1);

		tes_color=tcs_color;
	}
	
)glsl";

const std::string TESSELATION_FS=R"glsl(
	#version 400

	layout(early_fragment_tests) in;	// Z-coord of fragment can not be changed here, because visibility test executed before shading

	in vec3 tes_color;
	out vec3 color;

	void main(){
		color = tes_color;
	}
)glsl";

const std::string TRIANGLES_VS=R"glsl(
	#version 400 core

	layout(location = 0) in vec3 in_position;
	layout(location = 1) in vec3 in_color;
	uniform mat4 MVP;

	out vec3 vs_color;

	void main()
	{
		gl_Position=MVP*vec4(in_position, 1);
		vs_color=in_color;
	}
)glsl";

const std::string TRIANGLES_FS=R"glsl(
	#version 400

	layout(early_fragment_tests) in;	// Z-coord of fragment can not be changed here, because visibility test executed before shading

	in vec3 vs_color;
	out vec3 color;

	void main(){
		color = vs_color;
	}
)glsl";

struct OpenGL_Program {
	GLuint programID;
	GLuint objectsBufferID;
	GLuint colorBufferID;
	GLuint projectionMatrixID;
	GLuint trianglesProgram;
};

struct OpenGL_IDs {
	OpenGL_Program tesselationProgram;
	OpenGL_Program trianglesProgram;
};

void installShader(GLuint type, const std::string& shader, GLuint program) {
	GLuint shader_ID=glCreateShader(type);
	const char* adapter[1]={ shader.c_str() };
	int len_adapter[1]={ (int)shader.length() };
	glShaderSource(shader_ID, 1, adapter, 0);
	glCompileShader(shader_ID);
	GLint success=0;
	glGetShaderiv(shader_ID, GL_COMPILE_STATUS, &success);
	std::cout<<"COMPILE: "<<success<<" "<<GL_TRUE<<std::endl;
	if (success!=GL_TRUE) {
		GLint maxLength=0;
		glGetShaderiv(shader_ID, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> infoLog(maxLength);
		glGetShaderInfoLog(shader_ID, maxLength, &maxLength, &infoLog[0]);
		for (int i=0; i<maxLength; ++i) std::cout<<infoLog[i];
		std::cout<<std::endl;
	}
	glAttachShader(program, shader_ID);
}

void programLinkingErrorControl(GLuint program) {
	GLint isLinked=0;
	glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
	if (isLinked==GL_FALSE) {
		GLint maxLength=0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

		for (int i=0; i<maxLength; ++i)std::cout<<infoLog[i];
		std::cout<<std::endl;
	}
	else std::cout<<"LINK: "<<isLinked<<" "<<GL_TRUE<<std::endl;
}

OpenGL_Program installTesselationProgram() {
	OpenGL_Program ret;
	ret.programID=glCreateProgram();
	installShader(GL_VERTEX_SHADER, TESSELATION_VS, ret.programID);
	installShader(GL_TESS_CONTROL_SHADER, TESSELATION_TCS, ret.programID);
	installShader(GL_TESS_EVALUATION_SHADER, TESSELATION_TES, ret.programID);
	installShader(GL_FRAGMENT_SHADER, TESSELATION_FS, ret.programID);
	glLinkProgram(ret.programID);
	programLinkingErrorControl(ret.programID);
	return ret;
}

OpenGL_Program installTrianglesProgram() {
	OpenGL_Program ret;
	ret.programID=glCreateProgram();
	installShader(GL_VERTEX_SHADER, TRIANGLES_VS, ret.programID);
	installShader(GL_FRAGMENT_SHADER, TRIANGLES_FS, ret.programID);
	glLinkProgram(ret.programID);
	programLinkingErrorControl(ret.programID);
	return ret;
}

OpenGL_IDs installPrograms() {
	OpenGL_IDs ret;
	ret.tesselationProgram=installTesselationProgram();
	ret.trianglesProgram=installTrianglesProgram();
	return ret;
}

void initProgramBuffers(OpenGL_Program& program) {
	glGenBuffers(1, &program.objectsBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, program.objectsBufferID);
	glGenBuffers(1, &program.colorBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, program.colorBufferID);
	program.projectionMatrixID=glGetUniformLocation(program.programID, "MVP");
}

GLFWwindow* createWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	GLFWwindow* win=glfwCreateWindow(1280, 960, "HELLO", NULL, NULL);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(0);
	return win;
}

OpenGL_IDs initOpenGL() {
	glewInit();
	OpenGL_IDs ret=installPrograms();

	// create VAO
	GLuint VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);		
	// alloc buffers
	initProgramBuffers(ret.tesselationProgram);
	initProgramBuffers(ret.trianglesProgram);

	return ret;
}

void draw(OpenGL_IDs args);
void calc_FPS();

void drawingLoop(GLFWwindow* win, OpenGL_IDs args) {
	while (!glfwWindowShouldClose(win)) {
		draw(args);

		glfwSwapBuffers(win);
		calc_FPS();
		glfwPollEvents();
	}
}

std::vector<float> verts_tri{
	0.3f, 0.05f, -1.0f,
	0.3f, -0.05f, -1.0f,
	0.4f, -0.05f, -1.0f,

	0.3f, 0.05f, -1.0f,
	0.4f, 0.05f, -1.0f,
	0.4f, -0.05f, -1.0f
};

std::vector<float> colors_tri{
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f
};

std::vector<float> verts_square{
	-0.05f, -0.05f, -0.5f,
	0.05f, -0.05f, -0.5f,
	0.05f, 0.05f, -0.5f,
	-0.05f, 0.05f, -0.5f,
};

std::vector<float> verts_sphere{
	0.1f, 0.0f, -1.0f, 0.1f,
	-0.2f, 0.0f, -1.0f, 0.2f,
	0.5f, 0.5f, -1.5f, 0.1f
};

std::vector<float> colors_sphere{
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 1.0f
};

int i=0;		// global variable to determine draw loop index

void draw(OpenGL_IDs programs) {
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); GLEE();

	i=(i+1)%10000;
	verts_sphere[1]=i/10000.0/2;
	verts_sphere[5]=-i/10000.0/2;

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glm::mat4 mvp=glm::perspective(glm::radians(45.0), 1.0, 0.1, 20.0);
	//mvp=glm::ortho(-1, 1, -1, 1);

	{		// draw spheres using tesselation shader program
		glUseProgram(programs.tesselationProgram.programID); GLEE();
		glPatchParameteri(GL_PATCH_VERTICES, 1); GLEE();				// patch size can be specified once before fisrt draw, if will not be modified all over drawing loop
		glUniformMatrix4fv(programs.tesselationProgram.projectionMatrixID, 1, GL_FALSE, &mvp[0][0]); GLEE();	// can be once set in init() if not changed for each frame

		glEnableVertexAttribArray(0); GLEE();
		glBindBuffer(GL_ARRAY_BUFFER, programs.tesselationProgram.objectsBufferID); GLEE();
		glVertexAttribPointer(glGetAttribLocation(programs.tesselationProgram.programID, "in_position"), 4, GL_FLOAT, GL_FALSE, 0, 0); GLEE();
		glBufferData(GL_ARRAY_BUFFER, verts_sphere.size()*sizeof(float), &verts_sphere[0], GL_STATIC_DRAW); GLEE();

		glEnableVertexAttribArray(1); GLEE();
		glBindBuffer(GL_ARRAY_BUFFER, programs.tesselationProgram.colorBufferID); GLEE();
		glVertexAttribPointer(glGetAttribLocation(programs.tesselationProgram.programID, "in_color"), 3, GL_FLOAT, GL_FALSE, 0, 0); GLEE();
		glBufferData(GL_ARRAY_BUFFER, colors_sphere.size()*sizeof(float), &colors_sphere[0], GL_STATIC_DRAW); GLEE();

		glDrawArrays(GL_PATCHES, 0, 3); GLEE();

		glDisableVertexAttribArray(1); GLEE();
		glDisableVertexAttribArray(0); GLEE();
	}


	{	// draw triangles using only vertex shader and fragment shader program
		glUseProgram(programs.trianglesProgram.programID); GLEE();
		glUniformMatrix4fv(programs.trianglesProgram.projectionMatrixID, 1, GL_FALSE, &mvp[0][0]); GLEE();		// can be once set in init() if not changed for each frame

		glEnableVertexAttribArray(0); GLEE();
		glBindBuffer(GL_ARRAY_BUFFER, programs.trianglesProgram.objectsBufferID); GLEE();
		glVertexAttribPointer(glGetAttribLocation(programs.trianglesProgram.programID, "in_position"), 3, GL_FLOAT, GL_FALSE, 0, 0); GLEE();
		glBufferData(GL_ARRAY_BUFFER, verts_tri.size()*sizeof(float), &verts_tri[0], GL_STATIC_DRAW); GLEE();

		glEnableVertexAttribArray(1); GLEE();
		glBindBuffer(GL_ARRAY_BUFFER, programs.trianglesProgram.colorBufferID); GLEE();
		glVertexAttribPointer(glGetAttribLocation(programs.trianglesProgram.programID, "in_color"), 3, GL_FLOAT, GL_FALSE, 0, 0); GLEE();
		glBufferData(GL_ARRAY_BUFFER, colors_tri.size()*sizeof(float), &colors_tri[0], GL_STATIC_DRAW); GLEE();

		glDrawArrays(GL_TRIANGLES, 0, 6); GLEE();

		glDisableVertexAttribArray(1); GLEE();
		glDisableVertexAttribArray(0); GLEE();
	}
}

// those global variables are just for calc_FPS() function
std::chrono::time_point<std::chrono::high_resolution_clock> last=std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::nanoseconds(0));
int ticks=0;
int fps=-1;

void calc_FPS() {
	ticks++;
	auto now=std::chrono::high_resolution_clock::now();
	long long dur=std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count();
	if (dur<1000)return;
	fps=(int)std::round((float)ticks/dur*1000);
	last=now;
	ticks=0;
	std::cout<<"FPS: "<<fps<<std::endl;
}

int main(int argc, char** argv) {
	GLFWwindow* win=createWindow();
	OpenGL_IDs args=initOpenGL();
	drawingLoop(win, args);
	glfwDestroyWindow(win);

	return 0;
}