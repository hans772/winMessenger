#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include <stack>
#include <memory>
#include "util/config.hpp"

class Scene;

class SceneManager {
private:
	std::stack<std::unique_ptr<Scene>> scene_stack;

	SceneManager();

public:
	static SceneManager& get();

	bool should_quit = false;
	STARTUP_ITEM startup_selection;
	int selected_port;
	std::string selected_ip;

	void push(std::unique_ptr<Scene> scene);
	void replace(std::unique_ptr<Scene> scene);
	void pop();
	Scene& top();

	void operator=(const SceneManager&) = delete;

};

#endif