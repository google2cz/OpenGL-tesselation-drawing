#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<glm/common.hpp>
#include<glm/gtc/matrix_transform.hpp>

#include<GL/gl.h>
#include<GL/glu.h>

#include<iostream>
#include<thread>
#include<string>
#include<vector>
#define _USE_MATH_DEFINES
#include<math.h>

#if 1

const std::string VS=R"glsl(
	#version 400 core
	
	layout(location = 0) in vec4 vertex_position;

	void main()
	{
		gl_Position=vertex_position;
	}

)glsl";

const std::string TCS=R"glsl(
	#version 400 core

	layout(vertices=1) out;

	void main()
	{
		gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
		gl_TessLevelOuter[1]=gl_TessLevelOuter[3]=gl_TessLevelInner[1]=100;
		gl_TessLevelOuter[0]=gl_TessLevelOuter[2]=gl_TessLevelInner[0]=gl_TessLevelOuter[1]*2;
	}

)glsl";

const std::string TES=R"glsl(
	#version 400 core

	layout(quads, equal_spacing) in;

	uniform mat4 MVP;

	#define M_PI 3.1415926535897932384626433832795

	void main(){
		float psi = gl_TessCoord.x*2*M_PI;
		float phi = gl_TessCoord.y*M_PI;
		float r = gl_in[0].gl_Position.w;
		float dy = cos(phi)*r;
		r = sqrt(r*r-dy*dy);
		float dx = sin(psi)*r;
		float dz = cos(psi)*r;
		vec3 pt = gl_in[0].gl_Position.xyz+vec3(dx,dy,dz);
		gl_Position=MVP*vec4(normalize(pt), 1);
	}
	
)glsl";

#else

#define TRI_OR_QUAD

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

#endif

const std::string FS=R"glsl(
	#version 400 core

	out vec3 color;

	void main(){
		color = vec3(1,1,0);
	}
)glsl";

GLuint ProgramID;
GLuint VertexArrayID;
GLuint VertexBuffer;
GLuint MatrixID;

void setShader(GLuint type, const std::string& shader, GLuint program) {
	GLuint shader_ID=glCreateShader(type);
	const char* adapter[1]={ shader.c_str() };
	int len_adapter[1]={ (int)shader.length() };
	glShaderSource(shader_ID, 1, adapter, 0);
	glCompileShader(shader_ID);
	GLint success=0;
	glGetShaderiv(shader_ID, GL_COMPILE_STATUS, &success);
	std::cout<<"COMPILE: "<<success<<" "<<GL_TRUE<<std::endl;
	if(success!=GL_TRUE){
		GLint maxLength=0;
		glGetShaderiv(shader_ID, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> infoLog(maxLength);
		glGetShaderInfoLog(shader_ID, maxLength, &maxLength, &infoLog[0]);
		for (int i=0; i<maxLength; ++i) std::cout<<infoLog[i];
		std::cout<<"compilation failed, aborting."<<std::endl; abort();
	}
	glAttachShader(program, shader_ID);
}


void installShaders() {
	GLuint program=glCreateProgram();
	
	setShader(GL_VERTEX_SHADER, VS, program);
	setShader(GL_TESS_CONTROL_SHADER, TCS, program);
	setShader(GL_TESS_EVALUATION_SHADER, TES, program);
	setShader(GL_FRAGMENT_SHADER, FS, program);

	glLinkProgram(program);

	GLint isLinked=0;
	glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
	if (isLinked==GL_FALSE) {
		GLint maxLength=0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		// The maxLength includes the NULL character
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
		for (int i=0; i<maxLength; ++i) std::cout<<infoLog[i];
		std::cout<<"linking failed, aborting."<<std::endl; abort();
		//std::cout<<std::endl;
		//abort();
	}
	else std::cout<<"LINK: "<<isLinked<<" "<<GL_TRUE<<std::endl;
	glUseProgram(program);
	ProgramID=program;
}

void openGL_draw();

void GLFW_showWindow() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	GLFWwindow* win=glfwCreateWindow(1280, 960, "HELLO", NULL, NULL);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(0);

	glewInit();
	installShaders();

	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	glGenBuffers(1, &VertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);

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

	auto&& verts=verts_sphere;
	glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), &verts[0], GL_STATIC_DRAW);
	
	MatrixID=glGetUniformLocation(ProgramID, "MVP");

	while (!glfwWindowShouldClose(win)) {
		openGL_draw();

		glfwSwapBuffers(win);
		glfwPollEvents();
	}

	//glfwDestroyWindow(win);
}

void calc_FPS();

void openGL_draw() {
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	glPatchParameteri(GL_PATCH_VERTICES, 1);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glm::mat4 mvp=glm::perspective(glm::radians(45.0), 1.0, 0.1, 20.0);
	//mvp=glm::ortho(-1, 1, -1, 1);
	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &mvp[0][0]);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
	#ifdef TRI_OR_QUAD
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	#else
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	#endif

	//glDrawArrays(GL_PATCHES, 0, 3);
	//glDrawArrays(GL_PATCHES, 3, 3);
	//glDrawArrays(GL_PATCHES, 0, 4);
	glDrawArrays(GL_PATCHES, 0, 2);
	//glDrawArrays(GL_PATCHES, 1, 1);

	glDisableVertexAttribArray(0);

	GLuint error=glGetError();
	if (error!=GL_NO_ERROR)std::cout<<"ERR: 0x"<<std::hex<<error<<std::endl;

	glFinish();

	calc_FPS();
}

std::chrono::time_point<std::chrono::steady_clock> last=std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(0));
int ticks=0;
int fps=-1;

void calc_FPS() {
	ticks++;
	auto now=std::chrono::steady_clock::now();
	long long dur=std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count();
	if (dur<1000)return;
	fps=(int)std::round((float)ticks/dur*1000);
	last=now;
	ticks=0;
	std::cout<<fps<<std::endl;
}

int main(int argc, char** argv) {

	GLFW_showWindow();

	//system("pause");

	return 0;
}
