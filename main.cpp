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

const std::string VS=R"glsl(
	#version 400 core

	layout(location = 0) in vec4 in_position;
	layout(location = 1) in vec3 in_color;

	out vec3 vs_color;

	void main()
	{
		gl_Position=in_position;
		vs_color=in_color;
	}

)glsl";

const std::string TCS=R"glsl(
	#version 400 core

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

const std::string TES=R"glsl(
	#version 400 core

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

const std::string FS=R"glsl(
	#version 400 core

	layout(early_fragment_tests) in;	// Z-coord of fragment can not be changed here, because visibility test executed before shading

	in vec3 tes_color;
	out vec3 color;

	void main(){
		color = tes_color;
	}
)glsl";

#if 0

const std::string VS=R"glsl(
	#version 400 core
	
	layout(location = 0) in vec3 vertex_position;

	uniform mat4 MVP;

	void main()
	{
		gl_Position=MVP*vec4(vertex_position, 1);
	}

)glsl";

const std::string TCS=R"glsl(
	#version 400 core

	layout(vertices=4) out;

	void main()
	{
		gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
		gl_TessLevelOuter[0]=gl_TessLevelOuter[1]=gl_TessLevelOuter[2]=gl_TessLevelOuter[3]=5;
		gl_TessLevelInner[0]=gl_TessLevelInner[1]=7;
	}

)glsl";

const std::string TES=R"glsl(
	#version 400 core
#if 0

	layout (triangles, equal_spacing) in;

	void main(){ 
		vec3 a=gl_TessCoord.x*gl_in[0].gl_Position.xyz;
		vec3 b=gl_TessCoord.y*gl_in[1].gl_Position.xyz;
		vec3 c=gl_TessCoord.z*gl_in[2].gl_Position.xyz;
		gl_Position = vec4(normalize(a+b+c),1);
	}

#else
	
	layout (quads, equal_spacing) in;

	void main(){
		vec3 pos=gl_in[0].gl_Position.xyz;
		vec3 u=gl_in[1].gl_Position.xyz-gl_in[0].gl_Position.xyz;
		vec3 v=gl_in[3].gl_Position.xyz-gl_in[0].gl_Position.xyz;
		pos+=gl_TessCoord.x*u;
		pos+=gl_TessCoord.y*v;
		pos=normalize(pos);
		gl_Position = vec4(pos, 1);
	}

#endif
)glsl";

const std::string FS=R"glsl(
	#version 400 core

	layout(early_fragment_tests) in;

	out vec3 color;

	void main(){
		color = vec3(1,1,0);
	}
)glsl";

#endif

struct OpenGL_IDs {
	GLuint programID;
	GLuint spheresBufferID;
	GLuint colorBufferID;
	GLuint projectionMatrixID;
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


GLuint installShaders() {
	GLuint program=glCreateProgram();

	installShader(GL_VERTEX_SHADER, VS, program);
	installShader(GL_TESS_CONTROL_SHADER, TCS, program);
	installShader(GL_TESS_EVALUATION_SHADER, TES, program);
	installShader(GL_FRAGMENT_SHADER, FS, program);
	
	glLinkProgram(program);

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
	glUseProgram(program);
	return program;
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
	OpenGL_IDs ret;
	ret.programID=installShaders();

	// create VAO
	GLuint VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);		
	// alloc buffers
	glGenBuffers(1, &ret.spheresBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, ret.spheresBufferID);
	glGenBuffers(1, &ret.colorBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, ret.colorBufferID);
	ret.projectionMatrixID=glGetUniformLocation(ret.programID, "MVP");

	glPatchParameteri(GL_PATCH_VERTICES, 1);

	return ret;
}

void draw(OpenGL_IDs args);

void drawingLoop(GLFWwindow* win, OpenGL_IDs args) {
	while (!glfwWindowShouldClose(win)) {
		draw(args);

		glfwSwapBuffers(win);
		glfwPollEvents();
	}
}

std::vector<float> verts_tri{
	-0.05f, 0.05f, -0.5f,
	-0.05f, -0.05f, -0.5f,
	0.05f, -0.05f, -0.5f,

	-0.05f, 0.05f, -0.5f,
	0.05f, 0.05f, -0.5f,
	0.05f, -0.05f, -0.5f
};

std::vector<float> verts_square{
	-0.05f, -0.05f, -0.5f,
	0.05f, -0.05f, -0.5f,
	0.05f, 0.05f, -0.5f,
	-0.05f, 0.05f, -0.5f,
};

std::vector<float> verts_sphere{
	0.1f, 0.0f, -1.0f, 0.1f,
	-0.2f, 0.0f, -1.0f, 0.2f
};

std::vector<float> colors_sphere{
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
};

void calc_FPS();

int i=0;		// global variable to determine draw loop index

void draw(OpenGL_IDs args) {
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	i=(i+1)%10000;
	auto&& verts=verts_sphere;
	verts[1]=i/10000.0/2;
	verts[5]=-i/10000.0/2;
	auto&& colors=colors_sphere;

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glm::mat4 mvp=glm::perspective(glm::radians(45.0), 1.0, 0.1, 20.0);
	//mvp=glm::ortho(-1, 1, -1, 1);
	glUniformMatrix4fv(args.projectionMatrixID, 1, GL_FALSE, &mvp[0][0]);
	GLEE();

	glEnableVertexAttribArray(0);
	GLEE();
	glBindBuffer(GL_ARRAY_BUFFER, args.spheresBufferID);
	GLEE();
	glVertexAttribPointer(glGetAttribLocation(args.programID, "in_position"), 4, GL_FLOAT, GL_FALSE, 0, 0);
	GLEE();
	glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), &verts[0], GL_STATIC_DRAW);
	GLEE();

	glEnableVertexAttribArray(1);
	GLEE();
	glBindBuffer(GL_ARRAY_BUFFER, args.colorBufferID);
	GLEE();
	glVertexAttribPointer(glGetAttribLocation(args.programID, "in_color"), 3, GL_FLOAT, GL_FALSE, 0, 0);
	GLEE();
	glBufferData(GL_ARRAY_BUFFER, colors.size()*sizeof(float), &colors[0], GL_STATIC_DRAW);
	GLEE();

	glDrawArrays(GL_PATCHES, 0, 2);
	GLEE();

	glDisableVertexAttribArray(0);
	GLEE();
	glDisableVertexAttribArray(1);
	GLEE();

	glFinish();
	GLEE();

	calc_FPS();
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