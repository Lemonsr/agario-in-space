#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h> // to use pi

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

using namespace std;
using namespace glm;

// globals
bool animatingShipRotation;
float animAnglePerFrame;
float currFrameCount;
const float totalAnimationFrames = 10.0f;

float translationDist = 0.01f;

int score = 0;
bool isGameOver;
float defaultScaleFactor = 1.2f;

// initializes the vertices to fill the four corners of the window in GL's coord system.
CPU_Geometry gameObjectGeom() {
	CPU_Geometry retGeom;

	// vertex coordinates (for PART IV)
	retGeom.verts.push_back(vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(vec3(-1.f, -1.f, 0.f));
	retGeom.verts.push_back(vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(vec3(-1.f, 1.f, 0.f));
	retGeom.verts.push_back(vec3(1.f, -1.f, 0.f));
	retGeom.verts.push_back(vec3(1.f, 1.f, 0.f));

	// texture coordinates
	retGeom.texCoords.push_back(vec2(0.f, 1.f));
	retGeom.texCoords.push_back(vec2(0.f, 0.f));
	retGeom.texCoords.push_back(vec2(1.f, 0.f));
	retGeom.texCoords.push_back(vec2(0.f, 1.f));
	retGeom.texCoords.push_back(vec2(1.f, 0.f));
	retGeom.texCoords.push_back(vec2(1.f, 1.f));

	return retGeom;
}

float getCCWAngleFromXAxis(float x_distFromCentreOfShip, float y_distFromCentreOfShip, float arcTanAngle) {
	float angle;
	bool positiveX = x_distFromCentreOfShip > 0;
	bool positiveY = y_distFromCentreOfShip > 0;

	if (positiveX && positiveY) {
		// top right quad
		angle = arcTanAngle;
	}
	else if (!positiveX && positiveY) {
		// top left quad
		angle = M_PI + arcTanAngle;
	}
	else if (!positiveX && !positiveY) {
		// bottom left quad
		angle = M_PI + arcTanAngle;
	}
	else if (positiveX && !positiveY) {
		// bottom right quad
		angle = M_PI * 2 + arcTanAngle;
	}

	return angle;
}

float getNormalizedAngle(float angle) {
	if (angle >= M_PI * 2) {
		angle -= M_PI * 2;
	}
	return angle;
}

// each object has its own CPUgeom, GPUgeom and texture
struct GameGeometry {
	GameGeometry(CPU_Geometry cpg, string path, GLint interpolation)
		: cgeom(cpg)
		, ggeom()
		, texture(path, interpolation)
	{}
	CPU_Geometry cgeom;
	GPU_Geometry ggeom;
	Texture texture;
};

struct GameObject {
	GameObject(shared_ptr<GameGeometry> _g, float xCoord, float yCoord, vec2 scale) :
		// initialiser list to init members. its the only way to init const and reference members of objects, and members without default constructors.
		geometry(_g),
		position(xCoord, yCoord, 0.0f),
		originalPosition(xCoord, yCoord, 0.0f),
		theta(0),
		prevTheta(0), // for calculating rotation
		scale(scale),
		originalScale(scale),
		isActive(true),
		transformationMatrix(1.0f) // This constructor sets it as the identity matrix
	{ 
		// constructor body
		setTranslateMatrix();
		setScaleMatrix();
		setRotateMatrix();
		setTransformationMatrix();

		geometry->cgeom = gameObjectGeom();
		geometry->ggeom.setVerts(geometry->cgeom.verts);
		geometry->ggeom.setTexCoords(geometry->cgeom.texCoords);
	}

	shared_ptr<GameGeometry> geometry;

	mat4 transformationMatrix;
	mat4 translateMatrix, scaleMatrix, rotateMatrix; // transformation matrices

	vec3 position;
	const vec3 originalPosition;
	float theta, prevTheta; // Object's rotation
	// Alternatively, you could represent rotation via a normalized heading vec:
	// vec3 heading;
	vec2 scale;
	const vec2 originalScale;

	bool isActive; // if object is inactive, do not display/allow collision

	/* methods to update/reset gameobject */
	void setTranslateMatrix() {
		translateMatrix = { // glm uses the transposed matrix for translation
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{position.x, position.y, 0.0f, 1.0f}
		};
	}

	void setScaleMatrix() {
		scaleMatrix = {
			{scale.x, 0.0f, 0.0f, 0.0f},
			{0.0f, scale.y, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}
		};
	}

	void setRotateMatrix() {
		rotateMatrix = {
			{cos(theta), -sin(theta), 0.0f, 0.0f},
			{sin(theta), cos(theta), 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}
		};
	}

	mat4 setTransformationMatrix() {
		transformationMatrix = translateMatrix * rotateMatrix * scaleMatrix;
		return transformationMatrix;
	}

	void rotateGameObject(float newTheta) {
		theta = getNormalizedAngle(newTheta);
		//cout << "THETA: " << theta << endl;
		setRotateMatrix();
	}

	void translateGameObject(float xInc, float yInc) {
		position.x += xInc;
		position.y += yInc;
		setTranslateMatrix();
	}

	void scaleGameObject(float xInc, float yInc) {
		scale.x *= xInc;
		scale.y *= yInc;
		setScaleMatrix();
	}

	void setStatus(bool status) {
		isActive = status;
	}

	void resetAllStateToDefault() {
		theta = 0;
		prevTheta = 0;
		position = originalPosition;
		scale = originalScale;

		setTranslateMatrix();
		setScaleMatrix();
		setRotateMatrix();
		setTransformationMatrix();
		setStatus(true);
	}
};

void resetGame(GameObject& ship, vector<GameObject>& diamonds) {
	ship.resetAllStateToDefault();
	for (GameObject& diamond : diamonds) {
		diamond.resetAllStateToDefault();
	}
}

class MyCallbacks : public CallbackInterface { // look at callbackinterface for callback methods that we can use

public:
	MyCallbacks(GameObject& ship, vector<GameObject>& diamonds, ShaderProgram& shader) :
		ship_(ship),
		diamonds_(diamonds),
		shader(shader),
		location(vec2{0})
	{}

	virtual void keyCallback(int key, int scancode, int action, int mods) {
		float angleFromXAxis = ship_.theta + M_PI_2;
		//cout << angleFromXAxis << endl;
		if (key == GLFW_KEY_R && action == GLFW_PRESS) {
			// restart game
			isGameOver = false;
			score = 0;
			resetGame(ship_, diamonds_);
		}
		else if ((key == GLFW_KEY_UP ||	key == GLFW_KEY_W) && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			// move forward
			ship_.translateGameObject(-translationDist * cos(angleFromXAxis), translationDist * sin(angleFromXAxis));
		}
		else if ((key == GLFW_KEY_DOWN || key == GLFW_KEY_S) && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
			// move backward
			ship_.translateGameObject(translationDist * cos(angleFromXAxis), -translationDist * sin(angleFromXAxis));
		}
	}

	virtual void cursorPosCallback(double xpos, double ypos) {
		convertLocation(xpos, ypos);
		
		if (location != prevLocation) {
			//cout << location.x << " , " << location.y << endl;
			prevLocation = location;

			float x_distFromCentreOfShip = location.x - ship_.position[0];
			float y_distFromCentreOfShip = location.y - ship_.position[1];
			float arcTanAngle = atan(y_distFromCentreOfShip / x_distFromCentreOfShip);

			float angleFromYAxis = getCCWAngleFromXAxis(x_distFromCentreOfShip, y_distFromCentreOfShip, arcTanAngle) - M_PI_2;

			if (abs(angleFromYAxis - ship_.prevTheta) > M_PI) { // if angle of rotation is > half a full rotation, rotate opposite dir for shorter dist
				angleFromYAxis = -((M_PI * 2) - angleFromYAxis);
			}
			
			//cout << "angle from Yaxis: " << degrees(angleFromYAxis) << endl;
			//cout << "theta: " << degrees(ship_.prevTheta) << endl;

			//cout << "diff Yaxis to theta: " << degrees(abs(angleFromYAxis - ship_.prevTheta)) << endl;

			animatingShipRotation = true;
			float angleDiff = angleFromYAxis - ship_.prevTheta;
			animAnglePerFrame = angleDiff / totalAnimationFrames;
		}
	}

	// converts the location from a (0,0) by (800,800) window to the location in GL's coord system.
	void convertLocation(double x, double y) {
		vec2 s(x, y);
		s /= vec2(800.0);
		s += vec2(-0.5);
		s = vec2(s.x, -s.y);
		s *= 2.f;
		location = s;
	}

private:
	GameObject& ship_;
	vector<GameObject>& diamonds_;
	ShaderProgram& shader;
	vec2 location;
	vec2 prevLocation = {0.f, 0.f};
};

// rotate the ship to face the same direction as the mouse pointer
void animateShipRotation(GameObject& ship) {
	if (animatingShipRotation) {
		currFrameCount++;
		
		// animate small angle change per frame
		float newAngle = -(ship.prevTheta + animAnglePerFrame * currFrameCount);
		//cout << "incremental angle: " << degrees(newAngle) << endl;
		ship.rotateGameObject(newAngle);

		if (totalAnimationFrames == currFrameCount) { // completed animation
			animatingShipRotation = false;
			currFrameCount = 0;
			ship.prevTheta = -ship.theta;
		}
	}
}

void animateShipCollisionEvent(GameObject& ship, GameObject& diamond) {
	score++;
	diamond.setStatus(false); // "destroy" object
	ship.scaleGameObject(defaultScaleFactor, defaultScaleFactor);
	if (score == 3) {
		ship.setStatus(false);
		isGameOver = true;
	}
}

// collision detection: two axes overlap https://learnopengl.com/In-Practice/2D-Game/Collisions/Collision-detection
bool checkShipDiamondCollision(GameObject &ship, GameObject &diamond) {
	if (ship.isActive && diamond.isActive) {
		bool collisionX = ship.position.x + ship.scale.x >= diamond.position.x &&
			diamond.position.x + diamond.scale.x >= ship.position.x;
		bool collisionY = ship.position.y + ship.scale.y >= diamond.position.y &&
			diamond.position.y + diamond.scale.y >= ship.position.y;
		return collisionX && collisionY;
	}
	return false;
}

// passes the game object with the transformation matrix to the vertex shader to draw its correct position
void drawGameObject(GameObject& gameObject, ShaderProgram& shader) {
	if (gameObject.isActive) {
		gameObject.geometry->ggeom.bind();
		gameObject.geometry->texture.bind(); // push data from png (test.frag's uniform "sampler") into GPU mem

		// update uniform transformationMatrix value
		mat4 currTransformationMatrix = gameObject.setTransformationMatrix();
		GLint transformationMatrixLocation = glGetUniformLocation(shader.getProgramId(), "transformationMatrix");
		glUniformMatrix4fv(transformationMatrixLocation, 1, GL_FALSE, &currTransformationMatrix[0][0]); // &m[0][0]: dereferences first ele to get const address of vect content 

		glDrawArrays(GL_TRIANGLES, 0, 6);
		gameObject.geometry->texture.unbind();
	}
}

// inits 3 pickups for the game
vector<GameObject> init3Pickups(shared_ptr<GameGeometry> &pickupGeom, vec2 scale) {
	vector<GameObject> pickups;

	GameObject pickup1(pickupGeom, 0.5f, 0.5f, scale);
	GameObject pickup2(pickupGeom, -0.5f, 0.5f, scale);
	GameObject pickup3(pickupGeom, 0.5f, -0.5f, scale);

	pickups.push_back(pickup1);
	pickups.push_back(pickup2);
	pickups.push_back(pickup3);

	return pickups;
}

int main() {
	Log::debug("Starting main");

	// WINDOW
	glfwInit();
	Window window(800, 800, "CPSC 453"); // can set callbacks at construction if desired

	GLDebug::enable();

	// SHADERS
	ShaderProgram shader("shaders/test.vert", "shaders/test.frag");

	// GL_NEAREST looks a bit better for low-res pixel art than GL_LINEAR.
	// But for most other cases, you'd want GL_LINEAR interpolation.
	shared_ptr<GameGeometry> shipGeom = make_shared<GameGeometry>(
		gameObjectGeom(),
		"textures/ship.png",
		GL_NEAREST
	);
	shared_ptr<GameGeometry> diamondGeom = make_shared<GameGeometry>(
		gameObjectGeom(),
		"textures/diamond.png",
		GL_NEAREST
	);

	// Create gameobjects for ship and diamonds
	vector<GameObject> diamonds = init3Pickups(diamondGeom, { 0.07f, 0.07f });
	GameObject ship(shipGeom, 0.0f, 0.0f, { 0.09f, 0.06f }); // start at the center of the screen

	// CALLBACKS
	window.setCallbacks(make_shared<MyCallbacks>(ship, diamonds, shader)); // can also update callbacks to new ones

	// RENDER LOOP
	while (!window.shouldClose()) {
		glfwPollEvents();

		for (GameObject& diamond : diamonds) {
			if (checkShipDiamondCollision(ship, diamond)) {
				animateShipCollisionEvent(ship, diamond);
			}
		}

		shader.use();

		glEnable(GL_FRAMEBUFFER_SRGB);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		animateShipRotation(ship);

		drawGameObject(ship, shader);
		for (GameObject& diamond : diamonds) {
			drawGameObject(diamond, shader);
		}

		glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui

		// Starting the new ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// Putting the text-containing window in the top-left of the screen.
		ImGui::SetNextWindowPos(ImVec2(5, 5));

		// Setting flags
		ImGuiWindowFlags textWindowFlags =
			ImGuiWindowFlags_NoMove |				// text "window" should not move
			ImGuiWindowFlags_NoResize |				// should not resize
			ImGuiWindowFlags_NoCollapse |			// should not collapse
			ImGuiWindowFlags_NoSavedSettings |		// don't want saved settings mucking things up
			ImGuiWindowFlags_AlwaysAutoResize |		// window should auto-resize to fit the text
			ImGuiWindowFlags_NoBackground |			// window should be transparent; only the text should be visible
			ImGuiWindowFlags_NoDecoration |			// no decoration; only the text should be visible
			ImGuiWindowFlags_NoTitleBar;			// no title; only the text should be visible

		// Begin a new window with these flags. (bool *)0 is the "default" value for its argument.
		ImGui::Begin("scoreText", (bool *)0, textWindowFlags);

		// Scale up text a little, and set its value
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("Score: %d", score); // Second parameter gets passed into "%d"

		if (isGameOver) {
			ImGui::Text("Congratulations!\nYou have collected all the diamonds and won the game!\nPress R to restart the game.");
		}

		// End the window.
		ImGui::End();

		ImGui::Render();	// Render the ImGui window
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Some middleware thing

		window.swapBuffers();
	}

	// ImGui cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
