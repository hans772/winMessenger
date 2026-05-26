#ifndef SCENE_HPP
#define SCENE_HPP

#include <string>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"

struct Window {
	Window() = delete;
	Window(const char* name, ImGuiWindowFlags flags);
	Window(const char* name, ImVec4 rect, ImGuiWindowFlags flags);
	Window(const char* name, ImVec2 pos, ImGuiWindowFlags flags);
	~Window();
};

class Scene {
public:
	virtual void on_pause() = 0;
	virtual void on_resume() = 0;
	virtual void on_startup() = 0;
	virtual void on_exit() = 0;
	
	virtual void on_update() = 0;
	virtual void poll_events() = 0;

	virtual ~Scene() = default;
};

enum class STARTUP_ITEM {
	SERVER = 0,
	CLIENT = 1
};

class MenuScreen : public Scene {
private:
	int startup;
	
	int server_max_clients;
	std::string client_ip;

	std::string start_button_lbl;
	int port;

	void start_app();

public:
	MenuScreen();

	void on_pause() override;
	void on_resume() override;
	void on_startup() override;
	void on_exit() override;
	void on_update() override;
	
void poll_events() override; 

	~MenuScreen() override;
};

class ServerScreen : public Scene {
private:



public:
	ServerScreen();

	void on_pause() override;
	void on_resume() override;
	void on_startup() override;
	void on_exit() override;
	void on_update() override;
	void poll_events() override; 
	
	~ServerScreen() = default;
};

class ClientScreen {
	
};

#endif