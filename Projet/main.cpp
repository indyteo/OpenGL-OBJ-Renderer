//#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include "GLShader.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using glm::mat4;
using glm::vec2;
using glm::vec3;

extern "C" {
    uint32_t NvOptimusEnablement = 0x00000001;
}

using Color = vec3;

struct Vertex2 {
    vec2 position;
    Color color;
    vec2 texCoords;
};
struct Vertex3 {
	vec3 position;
	vec3 normal;
	vec2 texCoords;
};

const float PI = static_cast<float>(M_PI);
const float DEG_TO_RAD = PI / 180;
const float RAD_TO_DEG = 180 / PI;
const float EPSILON = 0.01f;
const float MOVEMENT_SPEED = 0.1f;

float cotan(float x) {
    return cos(x) / sin(x);
}

mat4 LookAt(vec3 position, vec3 target, vec3 up) {
	vec3 forward = glm::normalize(position - target);
	vec3 right = glm::normalize(glm::cross(up, forward));
	vec3 up2 = glm::cross(forward, right);
	return {
		right.x, up2.x, forward.x, 0,
		right.y, up2.y, forward.y, 0,
		right.z, up2.z, forward.z, 0,
		-glm::dot(right, position), -glm::dot(up2, position), -glm::dot(forward, position), 1
	};
}

struct Application;

struct Obj {
	Application& app;
	GLShader shader;
	GLuint buffers[3] = { 0, 0, 0 };
	GLuint vao = 0;
	GLuint texture = 0;
	int numOfIndices = 0;
	tinyobj::material_t material;

	vec3 scale = { 1, 1, 1 };
	float angle = 0;
	vec3 translation = { 0, 0, 0 };

	explicit Obj(Application& app) : app(app) {}

	void initialize(const char* shaderFileV, const char* shaderFileF, const std::string& objFile, const char* textureFile) {
		this->shader.LoadVertexShader(shaderFileV);
		this->shader.LoadFragmentShader(shaderFileF);
		this->shader.Create();

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(objFile)) {
			if (!reader.Error().empty()) {
				std::cerr << "TinyObjReader(" << objFile << "): " << reader.Error();
			}
			exit(1);
		}

		if (!reader.Warning().empty()) {
			std::cout << "TinyObjReader(" << objFile << "): " << reader.Warning();
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

		size_t verticesCount = 0;
		for (const auto& shape : shapes)
			for (const auto& num_face_vertice : shape.mesh.num_face_vertices)
				verticesCount += size_t(num_face_vertice);
		this->numOfIndices = int(verticesCount);
		float objVertices[8 * verticesCount];
		int objIndices[verticesCount];

		size_t vertex_offset = 0;
		size_t index_offset = 0;
		// Loop over shapes
		for (const auto& shape : shapes) {
			// Loop over faces(polygon)
			size_t shape_index_offset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
				auto fv = size_t(shape.mesh.num_face_vertices[f]);

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					tinyobj::index_t idx = shape.mesh.indices[shape_index_offset + v];

					// Vertex position
					objVertices[8 * vertex_offset + 0] = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					objVertices[8 * vertex_offset + 1] = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
					objVertices[8 * vertex_offset + 2] = -attrib.vertices[3 * size_t(idx.vertex_index) + 1];

					// Check if `normal_index` is zero or positive. negative = no normal data
					if (idx.normal_index >= 0) {
						objVertices[8 * vertex_offset + 3] = attrib.normals[3 * size_t(idx.normal_index) + 0];
						objVertices[8 * vertex_offset + 4] = attrib.normals[3 * size_t(idx.normal_index) + 2];
						objVertices[8 * vertex_offset + 5] = -attrib.normals[3 * size_t(idx.normal_index) + 1];
					}

					// Check if `texcoord_index` is zero or positive. negative = no texcoord data
					if (idx.texcoord_index >= 0) {
						objVertices[8 * vertex_offset + 6] = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						objVertices[8 * vertex_offset + 7] = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
					}

					// Optional: vertex colors
					// tinyobj::real_t r = attrib.colors[3 * size_t(idx.vertex_index) + 0];
					// tinyobj::real_t g = attrib.colors[3 * size_t(idx.vertex_index) + 1];
					// tinyobj::real_t b = attrib.colors[3 * size_t(idx.vertex_index) + 2];

					objIndices[index_offset] = int(vertex_offset);

					vertex_offset++;
					index_offset++;
				}
				shape_index_offset += fv;

				// per-face material
				//const auto& material = materials[shape.mesh.material_ids[f]];
			}
		}
		this->material = materials[0];

		uint32_t prog = this->getProgram();

		glGenBuffers(3, this->buffers);
		glGenVertexArrays(1, &this->vao);

		glBindVertexArray(this->vao);
		glBindBuffer(GL_ARRAY_BUFFER, this->buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, 4 * 8 * GLsizeiptr(verticesCount), objVertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * GLsizeiptr(verticesCount), objIndices, GL_STATIC_DRAW);
		const int32_t PROG_POSITION = glGetAttribLocation(prog, "position");
		const int32_t PROG_NORMAL = glGetAttribLocation(prog, "normal");
		const int32_t PROG_TEX_COORDS = glGetAttribLocation(prog, "texCoords");
		glEnableVertexAttribArray(PROG_POSITION);
		glEnableVertexAttribArray(PROG_NORMAL);
		glEnableVertexAttribArray(PROG_TEX_COORDS);
		glVertexAttribPointer(PROG_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3), (void*) offsetof(Vertex3, position));
		glVertexAttribPointer(PROG_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3), (void*) offsetof(Vertex3, normal));
		glVertexAttribPointer(PROG_TEX_COORDS, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3), (void*) offsetof(Vertex3, texCoords));

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		//glBindBuffer(GL_UNIFORM_BUFFER, this->buffers[2]);
		//glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(mat4), nullptr, GL_STREAM_DRAW);
		//glBindBufferBase(GL_UNIFORM_BUFFER, 0, this->buffers[2]);
		//glBindBuffer(GL_UNIFORM_BUFFER, 0);

		int w, h;
		uint8_t* data = stbi_load(textureFile, &w, &h, nullptr, STBI_rgb_alpha);
		if (!data) {
			std::cerr << "Failed to load texture: " << textureFile;
			exit(1);
		}

		glGenTextures(1, &this->texture);
		glBindTexture(GL_TEXTURE_2D, this->texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);
	}

	void render();

	void destroy() {
		glDeleteBuffers(2, this->buffers);
		glDeleteVertexArrays(1, &this->vao);
		glDeleteTextures(1, &this->texture);
		this->shader.Destroy();
	}

	inline uint32_t getProgram() {
		return this->shader.GetProgram();
	}
};

struct Application {
    int width;
    int height;
	GLShader basicShader;
	GLuint pausedBuffers[2] = { 0, 0 };
	GLuint pausedVao = 0;
	GLuint pausedTexture = 0;
	GLFWwindow* window = nullptr;
	double lastMouseX = 0;
	double lastMouseY = 0;
	float cameraPhi = PI / 2;
	float cameraTheta = 0;
	float cameraR = 50;
	vec3 target = { 0, 15, 0 };
	vec3 cameraPosition = { 0, 0, 0 };
	mat4 camera = {};
	mat4 projection = {};
	bool canMove = false;
	GLFWcursor* handCursor = nullptr;

	std::vector<Obj> objects;

    Application(int width, int height) : width(width), height(height) {}

    inline void setSize(int width, int height) {
        this->width = width;
        this->height = height;
    }

    bool initialize(GLFWwindow* window) {
		this->window = window;

        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
		glEnable(GL_FRAMEBUFFER_SRGB);
        std::cout << "Graphic card: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLEW: " << glewGetString(GLEW_VERSION) << std::endl;
        std::cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
        std::cout << "Extensions: " << glGetString(GL_EXTENSIONS) << std::endl;

        this->basicShader.LoadVertexShader("basic.vs.glsl");
        this->basicShader.LoadFragmentShader("basic.fs.glsl");
        this->basicShader.Create();
        uint32_t basic = this->getBasicProgram();

		/* OBJECTS */

		Obj table(*this);
		table.initialize("3d.vs.glsl", "3d.fs.glsl", "Obj/Meshes/dinertable.obj", "Obj/Textures/dinertable01_nv.png");
		table.translation = { 0, 0, 0 };
		table.scale = { 0.5, 0.5, 0.5 };
		this->objects.push_back(table);

		Obj apple(*this);
		apple.initialize("3d.vs.glsl", "3d_blink.fs.glsl", "Obj/Meshes/apple.obj", "Obj/Textures/apple.png");
		apple.translation = { 0, 34, 5 };
		this->objects.push_back(apple);

		Obj book(*this);
		book.initialize("3d_shake.vs.glsl", "3d.fs.glsl", "Obj/Meshes/Book.obj", "Obj/Textures/bookgeneric01.png");
		book.translation = { 15, 29, 0 };
		book.angle = 45;
		this->objects.push_back(book);

		Obj ragout(*this);
		ragout.initialize("3d.vs.glsl", "3d.fs.glsl", "Obj/Meshes/ragout.obj", "Obj/Textures/ratstew.png");
		ragout.translation = { -14, 30, -3 };
		this->objects.push_back(ragout);

		/* PAUSED */

		const Vertex2 pausedVertex[] = {
				{ { -0.265f, +0.8f }, { 1, 1, 1 }, { 0, 1 } },
				{ { -0.265f, +0.7f }, { 1, 1, 1 }, { 0, 0 } },
				{ { +0.265f, +0.7f }, { 1, 1, 1 }, { 1, 0 } },
				{ { +0.265f, +0.8f }, { 1, 1, 1 }, { 1, 1 } },
		};
		const unsigned int pausedIndices[] = { 0, 1, 2, 0, 2, 3 };

		glGenBuffers(2, this->pausedBuffers);
		glGenVertexArrays(1, &this->pausedVao);

		glBindVertexArray(this->pausedVao);
		glBindBuffer(GL_ARRAY_BUFFER, this->pausedBuffers[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2) * 4, pausedVertex, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->pausedBuffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 6, pausedIndices, GL_STATIC_DRAW);
		const int32_t BASIC_POSITION = glGetAttribLocation(basic, "position");
		const int32_t BASIC_COLOR = glGetAttribLocation(basic, "color");
		const int32_t BASIC_TEX_COORDS = glGetAttribLocation(basic, "texCoords");
		glEnableVertexAttribArray(BASIC_POSITION);
		glEnableVertexAttribArray(BASIC_COLOR);
		glEnableVertexAttribArray(BASIC_TEX_COORDS);
		glVertexAttribPointer(BASIC_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (void*) offsetof(Vertex2, position));
		glVertexAttribPointer(BASIC_COLOR, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (void*) offsetof(Vertex2, color));
		glVertexAttribPointer(BASIC_TEX_COORDS, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2), (void*) offsetof(Vertex2, texCoords));

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		int w, h;
		uint8_t* data = stbi_load("paused.png", &w, &h, nullptr, STBI_rgb_alpha);
		if (!data)
			return false;

		glGenTextures(1, &this->pausedTexture);
		glBindTexture(GL_TEXTURE_2D, this->pausedTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);

		/* GLFW CALLBACKS */

		this->handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
		glfwSetKeyCallback(this->window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
				auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
				app->canMove = !app->canMove;
			}
		});
		glfwSetMouseButtonCallback(this->window, [](GLFWwindow* window, int button, int action, int mods) {
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
				auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
				app->canMove = true;
			}
		});
		glfwSetScrollCallback(this->window, [](GLFWwindow* window, double xoffset, double yoffset) {
			auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
			if (app->canMove)
				app->cameraR = glm::clamp(app->cameraR - static_cast<float>(yoffset) * 0.5f, 1.f, 500.f);
		});
		glfwGetCursorPos(this->window, &this->lastMouseX, &this->lastMouseY);
		this->canMove = true;

		return true;
    }

	void renderPaused() {
		uint32_t basic = this->getBasicProgram();
		glUseProgram(basic);

		const int32_t BASIC_TIME = glGetUniformLocation(basic, "time");
		const int32_t BASIC_SAMPLER = glGetUniformLocation(basic, "sampler_");
		glUniform1f(BASIC_TIME, 0);
		glUniform1i(BASIC_SAMPLER, 0);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, this->pausedTexture);
		glBindVertexArray(this->pausedVao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
		glDisable(GL_BLEND);
	}

    void render() {
		bool clicked = glfwGetMouseButton(this->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		glfwSetCursor(this->window, clicked ? this->handCursor : nullptr);

		/* MOVEMENT */

		double mouseX, mouseY;
		glfwGetCursorPos(this->window, &mouseX, &mouseY);
		auto motionX = clicked ? static_cast<float>(this->lastMouseX - mouseX) : 0;
		auto motionY = clicked ? static_cast<float>(this->lastMouseY - mouseY) : 0;
		this->lastMouseX = mouseX;
		this->lastMouseY = mouseY;

		vec3 movement = { 0, 0, 0 };
		if (this->canMove) {
			if (glfwGetKey(this->window, GLFW_KEY_UP) == GLFW_PRESS)
				movement.x -= MOVEMENT_SPEED;
			if (glfwGetKey(this->window, GLFW_KEY_DOWN) == GLFW_PRESS)
				movement.x += MOVEMENT_SPEED;
			if (glfwGetKey(this->window, GLFW_KEY_SEMICOLON) == GLFW_PRESS)
				movement.y -= MOVEMENT_SPEED;
			if (glfwGetKey(this->window, GLFW_KEY_SPACE) == GLFW_PRESS)
				movement.y += MOVEMENT_SPEED;
			if (glfwGetKey(this->window, GLFW_KEY_RIGHT) == GLFW_PRESS)
				movement.z -= MOVEMENT_SPEED;
			if (glfwGetKey(this->window, GLFW_KEY_LEFT) == GLFW_PRESS)
				movement.z += MOVEMENT_SPEED;
		}

		if (this->canMove) {
			this->cameraPhi = glm::mod(this->cameraPhi - motionX * 0.005f + PI, 2 * PI) - PI;
			this->cameraTheta = glm::clamp(this->cameraTheta - motionY * 0.005f, -PI / 2 + EPSILON, PI / 2 - EPSILON);
		}
		glm::mat3 movementRotation = {
				cos(this->cameraPhi), 0, sin(this->cameraPhi),
				0, 1, 0,
				-sin(this->cameraPhi), 0, cos(this->cameraPhi),
		};
		this->target += movementRotation * movement;

		/* CAMERA */

        float aspect = static_cast<float>(this->width) / static_cast<float>(this->height);
        float near = 0.01, far = 500;
        float fovY = 55 * DEG_TO_RAD;
        float f = cotan(fovY / 2);
        this->projection = {
                f / aspect, 0, 0, 0,
                0, f, 0, 0,
                0, 0, (far + near) / (near - far), -1,
                0, 0, 2 * near * far / (near - far), 0,
        };
		vec3 rawCameraPosition = {
				this->cameraR * cos(this->cameraTheta) * cos(this->cameraPhi),
				this->cameraR * sin(this->cameraTheta),
				this->cameraR * cos(this->cameraTheta) * sin(this->cameraPhi)
		};
		this->cameraPosition = this->target + rawCameraPosition;
		this->camera = LookAt(this->cameraPosition, this->target, { 0, 1, 0 });

		/* DRAW */

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for (Obj& object : this->objects)
			object.render();

		if (!this->canMove)
			this->renderPaused();
	}

    void deinitialize() {
		for (Obj& object : this->objects)
			object.destroy();

		glDeleteBuffers(2, this->pausedBuffers);
		glDeleteVertexArrays(1, &this->pausedVao);
		glDeleteTextures(1, &this->pausedTexture);
        this->basicShader.Destroy();

		glfwDestroyCursor(this->handCursor);
    }

    inline uint32_t getBasicProgram() {
        return this->basicShader.GetProgram();
    }
};

void Obj::render() {
	auto time = static_cast<float>(glfwGetTime());

	uint32_t prog = this->getProgram();
	glUseProgram(prog);

	const int32_t PROG_TIME = glGetUniformLocation(prog, "time");
	const int32_t PROG_SAMPLER = glGetUniformLocation(prog, "sampler_");
	const int32_t PROG_LIGHT_DIRECTION = glGetUniformLocation(prog, "light.direction");
	const int32_t PROG_LIGHT_AMBIENT_COLOR = glGetUniformLocation(prog, "light.ambientColor");
	const int32_t PROG_LIGHT_DIFFUSE_COLOR = glGetUniformLocation(prog, "light.diffuseColor");
	const int32_t PROG_LIGHT_SPECULAR_COLOR = glGetUniformLocation(prog, "light.specularColor");
	const int32_t PROG_MATERIAL_AMBIENT_COLOR = glGetUniformLocation(prog, "material.ambientColor");
	const int32_t PROG_MATERIAL_DIFFUSE_COLOR = glGetUniformLocation(prog, "material.diffuseColor");
	const int32_t PROG_MATERIAL_SPECULAR_COLOR = glGetUniformLocation(prog, "material.specularColor");
	const int32_t PROG_SHININESS = glGetUniformLocation(prog, "shininess");
	glUniform1f(PROG_TIME, time);
	glUniform1i(PROG_SAMPLER, 0);
	glUniform3f(PROG_LIGHT_DIRECTION, 1, -1, -1);
	glUniform3f(PROG_LIGHT_AMBIENT_COLOR, 0.1, 0.1, 0.1);
	glUniform3f(PROG_LIGHT_DIFFUSE_COLOR, 1, 1, 1);
	glUniform3f(PROG_LIGHT_SPECULAR_COLOR, 0.5, 0.5, 0.5);
	glUniform3f(PROG_MATERIAL_AMBIENT_COLOR, this->material.ambient[0], this->material.ambient[1], this->material.ambient[2]);
	glUniform3f(PROG_MATERIAL_DIFFUSE_COLOR, this->material.diffuse[0], this->material.diffuse[1], this->material.diffuse[2]);
	glUniform3f(PROG_MATERIAL_SPECULAR_COLOR, this->material.specular[0], this->material.specular[1], this->material.specular[2]);
	glUniform1f(PROG_SHININESS, this->material.shininess);

	mat4 scaleMatrix = {
			this->scale.x, 0, 0, 0,
			0, this->scale.y, 0, 0,
			0, 0, this->scale.z, 0,
			0, 0, 0, 1,
	};
	mat4 rotationMatrix = {
			cos(this->angle), 0, sin(this->angle), 0,
			0, 1, 0, 0,
			-sin(this->angle), 0, cos(this->angle), 0,
			0, 0, 0, 1,
	};
	mat4 translationMatrix = {
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			this->translation.x, this->translation.y, this->translation.z, 1,
	};
	const int32_t PROG_VIEW = glGetUniformLocation(prog, "view");
	glUniform3f(PROG_VIEW, this->app.cameraPosition.x, this->app.cameraPosition.y, this->app.cameraPosition.z);
	mat4 transform = translationMatrix * rotationMatrix * scaleMatrix;
	mat4 transformNormal = glm::transpose(glm::inverse(transform));
	mat4 transformWithProjection = this->app.projection * this->app.camera * transform;
	//glBindBuffer(GL_UNIFORM_BUFFER, this->buffers[2]);
	//glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4), glm::value_ptr(transformNormal));
	//glBufferSubData(GL_UNIFORM_BUFFER, sizeof(mat4), sizeof(mat4), glm::value_ptr(transformWithProjection));
	//glBindBuffer(GL_UNIFORM_BUFFER, 0);

	const int32_t PROG_TRANSFORM_NORMAL = glGetUniformLocation(prog, "transformNormal");
	glUniformMatrix4fv(PROG_TRANSFORM_NORMAL, 1, GL_FALSE, glm::value_ptr(transformNormal));
	const int32_t PROG_TRANSFORM_WITH_PROJECTION = glGetUniformLocation(prog, "transformWithProjection");
	glUniformMatrix4fv(PROG_TRANSFORM_WITH_PROJECTION, 1, GL_FALSE, glm::value_ptr(transformWithProjection));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, this->texture);
	glBindVertexArray(this->vao);
	glDrawElements(GL_TRIANGLES, this->numOfIndices, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

int main() {
    Application app(1280, 960);
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(app.width, app.height, "Projet OpenGL - Joe BERTHELIN, Jenny CAO & Théo SZANTO - ESIEE Paris E4FI", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
	glfwSetWindowUserPointer(window, &app);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glewInit();

    if (!app.initialize(window)) {
        glfwTerminate();
        return -1;
    }

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        app.setSize(width, height);
        /* Render here */
        app.render();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    app.deinitialize();

    glfwTerminate();
    return 0;
}
