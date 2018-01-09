#include "pch.h"

#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Graphics1/Image.h>
#include <Kore/Graphics4/Graphics.h>
#include <Kore/Graphics4/PipelineState.h>
#include <Kore/Threads/Thread.h>
#include <Kore/Threads/Mutex.h>
#include "ObjLoader.h"

using namespace Kore;

class MeshData {
public:
	MeshData(const char* meshFile, const Graphics4::VertexStructure& structure) {
		mesh = loadObj(meshFile);
		
		vertexBuffer = new Graphics4::VertexBuffer(mesh->numVertices, structure);
		float* vertices = vertexBuffer->lock();
		for (int i = 0; i < mesh->numVertices; ++i) {
			vertices[i * 8 + 0] = mesh->vertices[i * 8 + 0];
			vertices[i * 8 + 1] = mesh->vertices[i * 8 + 1];
			vertices[i * 8 + 2] = mesh->vertices[i * 8 + 2];
			vertices[i * 8 + 3] = mesh->vertices[i * 8 + 3];
			vertices[i * 8 + 4] = 1.0f - mesh->vertices[i * 8 + 4];
			vertices[i * 8 + 5] = mesh->vertices[i * 8 + 5];
			vertices[i * 8 + 6] = mesh->vertices[i * 8 + 6];
			vertices[i * 8 + 7] = mesh->vertices[i * 8 + 7];
		}
		vertexBuffer->unlock();

		indexBuffer = new Graphics4::IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}
		indexBuffer->unlock();
	}

	Graphics4::VertexBuffer* vertexBuffer;
	Graphics4::IndexBuffer* indexBuffer;
	Mesh* mesh;
};

class MeshObject {
public:
	MeshObject(MeshData* mesh, Graphics4::Texture* image, vec3 position) : mesh(mesh), image(image) {
		M = mat4::Translation(position.x(), position.y(), position.z());
	}

	void render(Graphics4::TextureUnit tex) {
		Graphics4::setTexture(tex, image);
		Graphics4::setVertexBuffer(*mesh->vertexBuffer);
		Graphics4::setIndexBuffer(*mesh->indexBuffer);
		Graphics4::drawIndexedVertices();
	}

	void setTexture(Graphics4::Texture* tex) {
		image = tex;
	}

	Graphics4::Texture* getTexture() {
		return image;
	}

	mat4 M;
private:
	MeshData* mesh;
	Graphics4::Texture* image;
};

namespace {
	const int width = 1024;
	const int height = 768;
	double startTime;
	Graphics4::Shader* vertexShader;
	Graphics4::Shader* fragmentShader;
	Graphics4::PipelineState* pipeline;

	// null terminated array of MeshObject pointers
	MeshObject** objects;

	// The view projection matrix aka the camera
	mat4 P;
	mat4 V;

	vec3 position;
	bool up = false, down = false, left = false, right = false;

	Thread* streamingThread;
	Mutex streamMutex;

	// uniform locations - add more as you see fit
	Graphics4::TextureUnit tex;
	Graphics4::ConstantLocation pLocation;
	Graphics4::ConstantLocation vLocation;
	Graphics4::ConstantLocation mLocation;

	float angle;

	void stream(void*) {
		for (;;) {
			// to use a mutex, create a Mutex variable and call Create to initialize the mutex (see main()). Then you can use Lock/Unlock.
			streamMutex.lock();

			// load darmstadt.jpg files for near boxes
			// reload darmstadt.jpg for every box, pretend that every box has a different texture (I don't want to upload 100 images though)
			// feel free to create more versions of darmstadt.jpg at different sizes
			// always use less than 1 million pixels of texture data (the example code uses 100 16x16 textures - that's 25600 pixels, darmstadt.jpg is 512x512 aka 262144 pixels)

			// Beware, neither OpenGL nor Direct3D is thread safe - you can't just create a Texture in a second thread. But you can create a Kore::Image
			// in another thread, access it's pixels in the main thread and put them in a Kore::Texture using lock/unlock.

			streamMutex.unlock();
		}
	}

	void update() {
		float t = (float)(System::time() - startTime);

		const float speed = 0.1f;
		if (up) position.z() += speed;
		if (down) position.z() -= speed;
		if (left) position.x() -= speed;
		if (right) position.x() += speed;
		
		Graphics4::begin();
		Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, 0xff9999FF, 1.0f);
		
		Graphics4::setPipeline(pipeline);

	
		// set the camera
		P = mat4::Perspective(pi / 4.0f, (float)width / (float)height, 0.1f, 100);
		V = mat4::lookAt(position, vec3(0, 0, 1000), vec3(0, 1, 0));
		Graphics4::setMatrix(pLocation, P);
		Graphics4::setMatrix(vLocation, V);


		angle = t;


		//objects[0]->M = mat4::RotationY(angle) * mat4::RotationZ(Kore::pi / 4.0f);


		// iterate the MeshObjects
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// set the model matrix
			Graphics4::setMatrix(mLocation, (*current)->M);

			(*current)->render(tex);
			++current;
		}

		Graphics4::end();
		Graphics4::swapBuffers();
	}

	void mouseMove(int windowId, int x, int y, int movementX, int movementY) {

	}
	
	void mousePress(int windowId, int button, int x, int y) {

	}

	void mouseRelease(int windowId, int button, int x, int y) {

	}

	void keyDown(KeyCode code) {
		switch (code) {
		case KeyLeft:
			left = true;
			break;
		case KeyRight:
			right = true;
			break;
		case KeyUp:
			up = true;
			break;
		case KeyDown:
			down = true;
			break;
		}
	}

	void keyUp(KeyCode code) {
		switch (code) {
		case KeyLeft:
			left = false;
			break;
		case KeyRight:
			right = false;
			break;
		case KeyUp:
			up = false;
			break;
		case KeyDown:
			down = false;
			break;
		}
	}


	void init() {
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Graphics4::Shader(vs.readAll(), vs.size(), Graphics4::VertexShader);
		fragmentShader = new Graphics4::Shader(fs.readAll(), fs.size(), Graphics4::FragmentShader);

		// This defines the structure of your Vertex Buffer
		Graphics4::VertexStructure structure;
		structure.add("pos", Graphics4::Float3VertexData);
		structure.add("tex", Graphics4::Float2VertexData);
		structure.add("nor", Graphics4::Float3VertexData);

		pipeline = new Graphics4::PipelineState;
		pipeline->inputLayout[0] = &structure;
		pipeline->inputLayout[1] = nullptr;
		pipeline->vertexShader = vertexShader;
		pipeline->fragmentShader = fragmentShader;
		pipeline->depthMode = Graphics4::ZCompareLess;
		pipeline->depthWrite = true;
		pipeline->compile();

		tex = pipeline->getTextureUnit("tex");
		pLocation = pipeline->getConstantLocation("P");
		vLocation = pipeline->getConstantLocation("V");
		mLocation = pipeline->getConstantLocation("M");

		objects = new MeshObject*[101];
		for (int i = 0; i < 101; ++i) objects[i] = nullptr;

		MeshData* mesh = new MeshData("box.obj", structure);
		for (int y = 0; y < 10; ++y) {
			for (int x = 0; x < 10; ++x) {
				objects[y * 10 + x] = new MeshObject(mesh, new Graphics4::Texture("darmstadtmini.png", true), vec3((x - 5.0f) * 10, 0, (y - 5.0f) * 10));
			}
		}

		angle = 0.0f;

		Graphics4::setTextureAddressing(tex, Graphics4::U, Graphics4::Repeat);
		Graphics4::setTextureAddressing(tex, Graphics4::V, Graphics4::Repeat);
	}
}

int kore(int argc, char** argv) {
	Kore::System::init("Exercise 10", width, height);
	
	init();

	Kore::System::setCallback(update);

	startTime = System::time();
	
	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	streamMutex.create();
	Kore::threadsInit();
	streamingThread = Kore::createAndRunThread(stream, nullptr);

	Kore::System::start();
	
	return 0;
}
