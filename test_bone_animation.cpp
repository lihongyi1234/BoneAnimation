#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include "json/json.h"

#define MAX_BONE_INFLUENCE 4
#define MAX_BONES 100
#include <iostream>
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib,"jsoncpp.lib")

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 pos;\n"
"layout (location = 1) in ivec4 boneIds;\n"
"layout (location = 2) in vec4 weights;\n"
"const int MAX_BONES = 51;\n"
"const int MAX_BONE_INFLUENCE = 4;\n"
"uniform mat4 finalBonesMatrices[MAX_BONES];\n"
"void main()\n"
"{\n"
"vec4 totalPosition = vec4(0.0f);\n"
"for (int i = 0; i < MAX_BONE_INFLUENCE; i++)\n"
"{\n"
"	if (boneIds[i] == -1)\n"
"		continue;\n"
"	if (boneIds[i] >= MAX_BONES)\n"
"	{\n"
"		totalPosition = vec4(pos, 1.0f);\n"
"		break;\n"
"	}\n"
"	vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0f);\n"
"	totalPosition += localPosition * weights[i];\n"
"}\n"
"   gl_Position = vec4(totalPosition.x,totalPosition.y-0.9, totalPosition.z,totalPosition.w);\n"
"}\0";
const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = vec4(1.0f,0.5f,0.2f,1.0f);\n"
"}\n\0";

GLFWwindow* window = nullptr;


int initOpengl()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	return 0;
}

struct Vertex
{
	float position[3];
	int boneIds[MAX_BONE_INFLUENCE];
	float weights[MAX_BONE_INFLUENCE];
};


int main(int argc, char** argv)
{
	if (initOpengl() < 0) {
		return -1;
	}

	std::string json_fn = argv[1];
	//load json file
	std::ifstream ifs(json_fn);
	if (!ifs.is_open()) {
		std::cout << "error opening file!" << std::endl;
		return -1;
	}
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	std::string errs;
	bool ok = Json::parseFromStream(rbuilder, ifs, &root, &errs);
	if (!ok) {
		printf("parseFromStream failed\n");
		return -1;
	}
	
	int nb_triangles = root["f"].size() / 3;
	int nb_vertexs = root["pos"].size() / 3;
	int nb_bones = 51;
	int nb_frames = 25;
	std::vector<Vertex> vertices(nb_vertexs);
	unsigned int* indices = new unsigned int[nb_triangles * 3];
	std::vector<std::array<float, 12>> deformations;

	for (int i = 0; i < nb_vertexs; i++) {
		Vertex vertex;
		vertex.position[0] = root["pos"][3 * i].asFloat();
		vertex.position[1] = root["pos"][3 * i + 1].asFloat();
		vertex.position[2] = root["pos"][3 * i + 2].asFloat();

		vertex.boneIds[0] = root["indices"][4 * i].asInt();
		vertex.boneIds[1] = root["indices"][4 * i + 1].asInt();
		vertex.boneIds[2] = root["indices"][4 * i + 2].asInt();
		vertex.boneIds[3] = root["indices"][4 * i + 3].asInt();

		vertex.weights[0] = root["weight"][4 * i].asFloat();
		vertex.weights[1] = root["weight"][4 * i + 1].asFloat();
		vertex.weights[2] = root["weight"][4 * i + 2].asFloat();
		vertex.weights[3] = root["weight"][4 * i + 3].asFloat();
		
		vertices[i] = vertex;
	}

	for (int i = 0; i < nb_triangles*3;i++) {
		indices[i] = root["f"][i].asInt();
	}
	for (auto iter : root["deformation"]) { 
		std::array<float, 12> piece;
		for (int i = 0; i < piece.size(); i++) {
			piece[i] = iter[i].asFloat();
		}
		deformations.emplace_back(piece);
	}

	int frameIndex = 0;
	bool firstFrame = true;
	double last_time = double(clock()) / CLOCKS_PER_SEC * 1000.f;
	// build and compile our shader program
	// ------------------------------------
	// vertex shader
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// check for shader compile errors
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// fragment shader
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// check for shader compile errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// link shaders
	unsigned int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// check for linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	unsigned int VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, nb_triangles*3*sizeof(unsigned int), indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

	//ids
	glEnableVertexAttribArray(1);
	glVertexAttribIPointer(1, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIds));

	//weights
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weights));
	

	//note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//remember: do NOT unbind the EBO while a VAO is active as the bound element buffer object IS stored in the VAO; keep the EBO bound.
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
	// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
	glBindVertexArray(0);

	// uncomment this call to draw in wireframe polygons.
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// input
		// -----
		processInput(window);

		// render
		// ------
		glClearColor(1.f, 1.f, 1.f, 0.f);
		//glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// draw our first triangle
		glUseProgram(shaderProgram);

		//draw current frame
		double this_time = double(clock()) / CLOCKS_PER_SEC * 1000.f;
		if (!firstFrame && this_time - last_time > 40.f) {
			frameIndex++;
			frameIndex = frameIndex % nb_frames;
			last_time = this_time;
		}
		for (int i = 0; i < nb_bones; i++) {
			
			std::array<float, 12> deform = deformations[51 * frameIndex + i];
			float mat[16] = { deform[0],deform[3],deform[6],0,
				deform[1],deform[4],deform[7],0,
				deform[2],deform[5],deform[8],0,
				deform[9],deform[10],deform[11],1 };
			std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, name.c_str()), 1, GL_FALSE, &mat[0]);

			firstFrame = false;
		}


		glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
		//glDrawArrays(GL_POINT, 0, nb_vertexs);
		//glDrawArrays(GL_TRIANGLES, 0, nb_vertexs);
		glDrawElements(GL_TRIANGLES, nb_triangles*3, GL_UNSIGNED_INT, 0);
		//glBindVertexArray(0); // no need to unbind it every time 

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteProgram(shaderProgram);

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}