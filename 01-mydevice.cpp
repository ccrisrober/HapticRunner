#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <random>
//------------------------------------------------------------------------------
#include "chai3d.h"
#include "GL/glut.h"
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;

#define SPHERE_SIZE 0.05
#define UPDATE_TIME 0.550
#define MAX_LIVES 3
#define MAX_VEL 150.0
#define MAX_VEL_AUX MAX_VEL * 0.85
#define MIN_SCORE 0
#define MAX_COLUMNS 45
#define JUMP_TEXT "Jump!!"
#define BEND_TEXT "Bend!!"
#define CENTER_TEXT "Center"
#define LIVES_TEXT "Lives: "
#define DIE_TEXT "You died. Press R to continue ..."
#define SCORE_TEXT "Score: "
#define PLAYER_ICON "o"

// Sounds
struct {
	cAudioDevice* audioDevice;
	cAudioSource* audioSource;
	cAudioBuffer* vidasBuffer[MAX_LIVES];
	cAudioBuffer *jumpBuffer, *downBuffer;
} sounds;

int lives = MAX_LIVES;

bool initRun = false;

struct Column {
	char a, b, c;
	int type;

	Column(char a, char b, char c, int type) {
		this->a = a;
		this->b = b;
		this->c = c;
		this->type = type;
	}
};

struct {
	int x = 2;
	int y = 1;
} xy;

std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<int> dist(0, 2);

std::vector<Column> columns;

int score = 0;

cWorld* world;
cCamera* camera;
cDirectionalLight *light;
cHapticDeviceHandler* handler;
cGenericHapticDevicePtr hapticDevice;
cVector3d hapticDevicePosition;
cShapeSphere* cursor;

bool simulationRunning = false;

bool simulationFinished = true;

int screenW;
int screenH;
int windowW;
int windowH;
int windowPosX;
int windowPosY;

void resizeWindow(int w, int h);

void updateGraphics(void);

void graphicsTimer(int data);

void close(void);

void updateHaptics(void);

void gotoxy(int x, int y) {
	COORD pos = { x, y };
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleCursorPosition(output, pos);
}


void draw() {
	int size = std::min(30, (int)columns.size());
	for (int i = 0; i < size; i++) {
		std::cerr << columns[i].a;
	}
	std::cerr << std::endl;
	for (int i = 0; i < size; i++) {
		std::cerr << columns[i].b;
	}
	std::cerr << std::endl;
	for (int i = 0; i < size; i++) {
		std::cerr << columns[i].c;
	}
	std::cerr << std::endl;
	static int old_post = 0;
	if (columns[3].type == 1) {
		if (old_post != 1) {
			sounds.audioSource->setAudioBuffer(sounds.jumpBuffer);
			sounds.audioSource->play();
		}
		old_post = 1;
		std::cout << JUMP_TEXT << columns[2].b << std::endl;
	}
	else if (columns[3].type == 2) {
		if (old_post != 2) {
			sounds.audioSource->setAudioBuffer(sounds.downBuffer);
			sounds.audioSource->play();
		}
		old_post = 2;
		std::cout << BEND_TEXT << columns[2].b << std::endl;
	}
	else if (columns[4].type == -1) {
		sounds.audioSource->setAudioBuffer(sounds.downBuffer);
		sounds.audioSource->play();
		old_post = 0;
		std::cout << CENTER_TEXT << columns[2].b << std::endl;
	}
	else if (columns[4].type == -2) {
		sounds.audioSource->setAudioBuffer(sounds.jumpBuffer);
		sounds.audioSource->play();
		old_post = 0;
		std::cout << CENTER_TEXT << columns[2].b << std::endl;
	}
	else if (columns[4].type == 0) {
		old_post = 0;
	}

	if (lives > 0)
		std::cout << LIVES_TEXT << lives << std::endl;
	else {
		std::cout << DIE_TEXT << std::endl;
	}
	std::cout << SCORE_TEXT << score << std::endl;
	gotoxy(xy.x, xy.y);
	std::cout << PLAYER_ICON;
}

bool nextNormalBlock, nextnextNormalBlock = true;

void addBlockJump() {
	columns.push_back(Column(' ', ' ', '#', 0));
	columns.push_back(Column(' ', ' ', '#', 1));
	columns.push_back(Column(' ', '#', '#', 1));
	columns.push_back(Column(' ', '#', '#', -1));
	columns.push_back(Column(' ', ' ', '#', 0));
	columns.push_back(Column(' ', ' ', '#', 0));
	nextNormalBlock = false;
	nextnextNormalBlock = false;
}

void addBlockSlide() {
	columns.push_back(Column('#', ' ', ' ', 0));
	columns.push_back(Column('#', ' ', ' ', 2));
	columns.push_back(Column('#', '#', ' ', 2));
	columns.push_back(Column('#', '#', ' ', -2));
	columns.push_back(Column('#', ' ', ' ', 0));
	columns.push_back(Column('#', ' ', ' ', 0));
	nextNormalBlock = false;
	nextnextNormalBlock = false;
}

void addNormalBlock() {
	columns.push_back(Column(' ', ' ', '#', 0));
	columns.push_back(Column(' ', ' ', '#', 0));
	nextNormalBlock = true;
	nextnextNormalBlock = !nextnextNormalBlock;
}
void update() {
	if (columns.size() < MAX_COLUMNS) {
		if (nextNormalBlock && !nextnextNormalBlock) {
			int randObs = dist(mt);
			if (randObs == 0) {
				addBlockJump();
			}
			else if (randObs == 1) {
				addBlockSlide();
			}
			else {
				addNormalBlock();
			}
		}
		else {
			addNormalBlock();
		}
	}
	Column col = columns[xy.x];
	if (
		(xy.y == 0 && (col.a == '#' && columns[xy.x + 1].a == '#')) ||
		(xy.y == 1 && (col.b == '#' && columns[xy.x + 1].b == '#')) ||
		(xy.y == 2 && (col.c == '#' && columns[xy.x + 1].c == '#'))
	) {
		sounds.audioSource->setAudioBuffer(sounds.vidasBuffer[lives-1]);
		sounds.audioSource->play();
		lives--;
	}
	columns.erase(columns.begin());
	if (initRun) {
		score++;
	}
	if (col.b == '0' && columns[xy.x + 1].b == ' ') {
		initRun = true;
	}
}

void loadGL(int argc, char* argv[]) {
	// initialize GLUT
	glutInit(&argc, argv);

	// retrieve  resolution of computer display and position window accordingly
	screenW = glutGet(GLUT_SCREEN_WIDTH);
	screenH = glutGet(GLUT_SCREEN_HEIGHT);
	windowW = (int)(0.8 * screenH);
	windowH = (int)(0.5 * screenH);
	windowPosY = (screenH - windowH) / 2;
	windowPosX = windowPosY;

	// initialize the OpenGL GLUT window
	glutInitWindowPosition(windowPosX, windowPosY);
	glutInitWindowSize(windowW, windowH);

	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	// create display context
	glutCreateWindow(argv[0]);

	// initialize GLEW library
	glewInit();

	// setup GLUT options
	glutDisplayFunc(updateGraphics);
	glutReshapeFunc(resizeWindow);
	glutSetWindowTitle("DTHV 1");
}

void loadHaptics() {
	// create a haptic device handler
	handler = new cHapticDeviceHandler();

	// get a handle to the first haptic device
	handler->getDevice(hapticDevice, 0);

	// open a connection to haptic device
	hapticDevice->open();

	// calibrate device (if necessary)
	hapticDevice->calibrate();
}

void loadSounds() {
	sounds.audioDevice = new cAudioDevice();
	// Attach audio device to camera
	camera->attachAudioDevice(sounds.audioDevice);

	for (size_t i = 0; i < MAX_LIVES; i++) {
		sounds.vidasBuffer[i] = sounds.audioDevice->newAudioBuffer();
		sounds.vidasBuffer[i]->loadFromFile("../../../bin/resources/sounds/vidas_" + std::to_string(i) + ".wav");
	}

	sounds.jumpBuffer = sounds.audioDevice->newAudioBuffer();
	bool loaded = sounds.jumpBuffer->loadFromFile("../../../bin/resources/sounds/Salta.wav");

	sounds.downBuffer = sounds.audioDevice->newAudioBuffer();
	loaded = sounds.downBuffer->loadFromFile("../../../bin/resources/sounds/Abajo.wav");

	sounds.audioSource = sounds.audioDevice->newAudioSource();
}

cShapeSphere *box, *box2;
std::vector<cShapeSphere*> fila1;
std::vector<cShapeSphere*> fila2;
std::vector<cShapeSphere*> fila3;

void loadWorld() {
	// create a new world.
	world = new cWorld();

	// set the background color of the environment
	world->m_backgroundColor.setBlack();

	// create a camera and insert it into the virtual world
	camera = new cCamera(world);
	world->addChild(camera);

	// position and orient the camera
	camera->set(cVector3d(0.5, 0.0, 0.0),    // camera position (eye)
		cVector3d(0.0, 0.0, 0.0),    // look at position (target)
		cVector3d(0.0, 0.0, 1.0));   // direction of the (up) vector

	// set the near and far clipping planes of the camera
	camera->setClippingPlanes(0.01, 10.0);

	// create a directional light source
	light = new cDirectionalLight(world);

	// insert light source inside world
	world->addChild(light);

	// enable light source
	light->setEnabled(true);

	// define direction of light beam
	light->setDir(-1.0, 0.0, 0.0);

	// create a sphere (cursor) to represent the haptic device
	cursor = new cShapeSphere(SPHERE_SIZE);
	auto pos = cursor->getLocalPos();
	pos.add(chai3d::cVector3d(0, SPHERE_SIZE * 2.0 + (SPHERE_SIZE * -2.0 * 2.0), 0));
	cursor->setLocalPos(pos);
	cursor->m_material->setGreenMediumAquamarine();

	// insert cursor inside world
	world->addChild(cursor);

	for (int i = -3; i < 2; i++) {
		box = new cShapeSphere(SPHERE_SIZE);
		auto pos = box->getLocalPos();
		pos.add(chai3d::cVector3d(0, SPHERE_SIZE * 2.0 + (SPHERE_SIZE * i * 2.0), SPHERE_SIZE * 2.0));
		box->setLocalPos(pos);
		box->m_material->setRed();
		world->addChild(box);
		fila1.push_back(box);
	}
	for (int i = -3; i < 2; i++) {
		box = new cShapeSphere(SPHERE_SIZE);
		auto pos = box->getLocalPos();
		pos.add(chai3d::cVector3d(0, SPHERE_SIZE * 2.0 + (SPHERE_SIZE * i * 2.0), - SPHERE_SIZE * 2.0));
		box->setLocalPos(pos);
		box->m_material->setRed();
		world->addChild(box);
		fila3.push_back(box);
	}

}

cPrecisionClock simClock;

void reset() {
	lives = MAX_LIVES;
	initRun = false;
	score = MIN_SCORE;
	columns.clear();
	for (int i = 0; i < MAX_COLUMNS; i++) {
		columns.push_back(Column(' ', ' ', ' ', 0));
	}
	columns[MAX_COLUMNS - 7] = Column('3', '3', '3', 0);
	columns[MAX_COLUMNS - 7] = Column('3', '3', '3', 0);
	columns[MAX_COLUMNS - 6] = Column('2', '2', '2', 0);
	columns[MAX_COLUMNS - 5] = Column('2', '2', '2', 0);
	columns[MAX_COLUMNS - 4] = Column('1', '1', '1', 0);
	columns[MAX_COLUMNS - 3] = Column('1', '1', '1', 0);
	columns[MAX_COLUMNS - 2] = Column('0', '0', '0', 0);
	columns[MAX_COLUMNS - 1] = Column('0', '0', '0', 0);

	columns.push_back(Column('#', ' ', '#', 0));
	simClock.start();
}

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 'r':
		if (lives == 0) {
			reset();
			glutPostRedisplay();
		}
		break;
	}
}

int main(int argc, char* argv[]) {
	// Load all (gl, scene and haptics)
	loadGL(argc, argv);
	loadWorld();
	loadHaptics();
	loadSounds();
	// TODO: Close audioDevice!!

	// Start simulation
	cThread* hapticsThread = new cThread();
	hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);

	glutKeyboardFunc(keyboard);
	reset();

	glutTimerFunc(50, graphicsTimer, 0);
	glutMainLoop();

	close();

	return (0);
}

void resizeWindow(int w, int h) {
	windowW = w;
	windowH = h;
}

void close(void) {
	// stop the simulation
	simulationRunning = false;

	// wait for graphics and haptics loops to terminate
	while (!simulationFinished) { cSleepMs(100); }

	// close haptic device
	hapticDevice->close();
}

void graphicsTimer(int data) {
	if (simulationRunning) {
		glutPostRedisplay();
	}

	glutTimerFunc(50, graphicsTimer, 0);
}

void updateGraphics(void) {
	// render world
	camera->renderView(windowW, windowH);

	// swap buffers
	glutSwapBuffers();

	// check for any OpenGL errors
	GLenum err;
	err = glGetError();
	if (err != GL_NO_ERROR) std::cout << "Error:  %s\n" << gluErrorString(err);
}


void updateHaptics(void) {
	// simulation in now running
	simulationRunning = true;
	simulationFinished = false;

	// main haptic simulation loop
	while (simulationRunning) {
		// 1. Read position of haptic device
		cVector3d position;
		hapticDevice->getPosition(position);

		// Update position of cursor
		cursor->setLocalPos(position);

		if (position.z() < -0.01) {
			xy.y = 2;
		}
		//mover newHapticDevice->setForce(cVector3d(0, 0, -10));
		else if (position.z() > 0.01)
		{
			xy.y = 0;
		}
		else
		{
			xy.y = 1;
		}

		// 2. Compute interaction forces
		//tool->computeInteractionForces();

		// 3. Send forces to device
		//tool->applyForces();

		//if (useDamping)
		{
			// read position 
			cVector3d position;
			hapticDevice->getPosition(position);

			// read orientation 
			cMatrix3d rotation;
			hapticDevice->getRotation(rotation);

			// read gripper position
			double gripperAngle;
			hapticDevice->getGripperAngleRad(gripperAngle);

			// read linear velocity 
			cVector3d linearVelocity;
			hapticDevice->getLinearVelocity(linearVelocity);

			// read angular velocity
			cVector3d angularVelocity;
			hapticDevice->getAngularVelocity(angularVelocity);

			// read gripper angular velocity
			double gripperAngularVelocity;
			hapticDevice->getGripperAngularVelocity(gripperAngularVelocity);

			cVector3d force(0, 0, 0);
			cVector3d torque(0, 0, 0);
			double gripperForce = 0.0;

			cHapticDeviceInfo info = hapticDevice->getSpecifications();

			// compute linear damping force
			double Kv = 1.0 * info.m_maxLinearDamping;
			cVector3d forceDamping = -Kv * linearVelocity;
			force.add(forceDamping);

			// compute angular damping force
			double Kvr = 1.0 * info.m_maxAngularDamping;
			cVector3d torqueDamping = -Kvr * angularVelocity;
			torque.add(torqueDamping);

			// compute gripper angular damping force
			double Kvg = 1.0 * info.m_maxGripperAngularDamping;
			gripperForce = gripperForce - Kvg * gripperAngularVelocity;

			// send computed force, torque, and gripper force to haptic device	
			hapticDevice->setForceAndTorqueAndGripperForce(force, torque, gripperForce);
		}

		// read the time increment in seconds
		double timeInterval = simClock.getCurrentTimeSeconds();

		if (lives > 0 && timeInterval > UPDATE_TIME - UPDATE_TIME * ((score > MAX_VEL ? MAX_VEL : score) / MAX_VEL_AUX)) {
			system("cls");
			simClock.reset();

			/*for (int i = 0; i < fila1.size(); i++) {
				auto pos = fila1[i]->getLocalPos();
				pos.add(0, -SPHERE_SIZE*2.0, 0);
				if(pos.y() < )
				fila1[i]->setLocalPos(pos);
			}*/

			update();
			draw();
		}
	}

	// exit haptics thread
	simulationFinished = true;
}