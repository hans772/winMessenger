#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include <stack>
#include <memory>

class Scene;

class SceneManager {
private:
	std::stack<std::unique_ptr<Scene>> scene_stack;

	SceneManager();

public:
	static SceneManager& get();

	void push(std::unique_ptr<Scene> scene);
	void replace(std::unique_ptr<Scene> scene);
	void pop();
	Scene& top();

	void operator=(const SceneManager&) = delete;

};

#endif