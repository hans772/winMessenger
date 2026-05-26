#include <scenes/scene_manager.hpp>
#include "scenes/scenes.hpp"
#include <cassert>

SceneManager::SceneManager() {}

SceneManager& SceneManager::get() {
	static SceneManager sm_inst;
	return sm_inst;
}

void SceneManager::pop() {
	if (!scene_stack.empty()) {
		scene_stack.top()->on_exit();
		scene_stack.pop();
		if (!scene_stack.empty()) scene_stack.top()->on_resume();
	}
}

void SceneManager::push(std::unique_ptr<Scene> scene) {
	if (!scene_stack.empty()) scene_stack.top()->on_pause();
	scene_stack.push(std::move(scene));
	scene_stack.top()->on_startup();
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
	pop();
	scene_stack.push(std::move(scene));
	scene_stack.top()->on_startup();
}

Scene& SceneManager::top() {
	assert(!scene_stack.empty() && "Scene Stack was Empty!");
	return *scene_stack.top().get();
}