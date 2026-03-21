#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <DeclarativeSoundEngine/AudioConfig.hpp>
#include <DeclarativeSoundEngine/AudioManagerAPI.hpp>
#include <DeclarativeSoundEngine/Log.hpp>

namespace fs = std::filesystem;

namespace {

struct CliOptions {
	AudioConfig config{};
	fs::path assetPath = fs::current_path();
	fs::path behaviorPath = fs::current_path() / "behaviors";
};

void PrintUsage(const char* exeName)
{
	std::cout
		<< "Usage: " << exeName << " [options]\n"
		<< "  --assets <path>        Asset root directory\n"
		<< "  --behaviors <path>     Behavior directory\n"
		<< "  --backend <name>       miniaudio | unity | stub\n"
		<< "  --buffer-frames <n>    Device period size\n"
		<< "  --sample-rate <n>      Output sample rate\n"
		<< "  --channels <n>         Output channel count\n"
		<< "  --help                 Show this help\n";
}

bool TryParseBackend(const std::string& value, AudioBackend& out)
{
	if (value == "miniaudio") {
		out = AudioBackend::Miniaudio;
		return true;
	}
	if (value == "unity") {
		out = AudioBackend::Unity;
		return true;
	}
	if (value == "stub") {
		out = AudioBackend::Stub;
		return true;
	}
	return false;
}

bool ParseArgs(int argc, char** argv, CliOptions& options, bool& shouldExit)
{
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];

		if (arg == "--help" || arg == "-h") {
			PrintUsage(argv[0]);
			shouldExit = true;
			return false;
		}
		if (i + 1 >= argc) {
			std::cerr << "Missing value for " << arg << '\n';
			return false;
		}

		const std::string value = argv[++i];
		if (arg == "--assets") {
			options.assetPath = value;
		}
		else if (arg == "--behaviors") {
			options.behaviorPath = value;
		}
		else if (arg == "--backend") {
			if (!TryParseBackend(value, options.config.backend)) {
				std::cerr << "Unknown backend: " << value << '\n';
				return false;
			}
		}
		else if (arg == "--buffer-frames") {
			options.config.bufferFrames = static_cast<std::uint32_t>(std::stoul(value));
		}
		else if (arg == "--sample-rate") {
			options.config.sampleRate = static_cast<std::uint32_t>(std::stoul(value));
		}
		else if (arg == "--channels") {
			options.config.channels = static_cast<std::uint32_t>(std::stoul(value));
		}
		else {
			std::cerr << "Unknown option: " << arg << '\n';
			return false;
		}
	}

	return true;
}

void PrintPromptHelp()
{
	std::cout
		<< "Commands:\n"
		<< "  help\n"
		<< "  tag <entity> <tag>\n"
		<< "  clear <entity> <tag>\n"
		<< "  transient <entity> <tag>\n"
		<< "  float <entity> <key> <value>\n"
		<< "  string <entity> <key> <value...>\n"
		<< "  pos <entity> <x> <y> <z>\n"
		<< "  quat <entity> <a> <b> <c> <d>\n"
		<< "  transform <entity> <x> <y> <z> <a> <b> <c> <d>\n"
		<< "  dump\n"
		<< "  exit\n";
}

bool ProcessCommand(const std::string& line)
{
	std::istringstream input(line);
	std::string command;
	input >> command;

	if (command.empty()) {
		return true;
	}
	if (command == "help") {
		PrintPromptHelp();
		return true;
	}
	if (command == "exit" || command == "quit") {
		return false;
	}
	if (command == "dump") {
		AudioManager_DebugPrintState();
		return true;
	}
	if (command == "tag") {
		std::string entity;
		std::string tag;
		if (input >> entity >> tag) {
			AudioManager_SetTag(entity.c_str(), tag.c_str());
			return true;
		}
	}
	else if (command == "clear") {
		std::string entity;
		std::string tag;
		if (input >> entity >> tag) {
			AudioManager_ClearTag(entity.c_str(), tag.c_str());
			return true;
		}
	}
	else if (command == "transient") {
		std::string entity;
		std::string tag;
		if (input >> entity >> tag) {
			AudioManager_SetTransientTag(entity.c_str(), tag.c_str());
			return true;
		}
	}
	else if (command == "float") {
		std::string entity;
		std::string key;
		float value = 0.0f;
		if (input >> entity >> key >> value) {
			AudioManager_SetFloatValue(entity.c_str(), key.c_str(), value);
			return true;
		}
	}
	else if (command == "string") {
		std::string entity;
		std::string key;
		std::string value;
		if (input >> entity >> key && std::getline(input >> std::ws, value)) {
			AudioManager_SetStringValue(entity.c_str(), key.c_str(), value.c_str());
			return true;
		}
	}
	else if (command == "pos") {
		std::string entity;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (input >> entity >> x >> y >> z) {
			AudioManager_SetVectorValue(entity.c_str(), "position", x, y, z);
			return true;
		}
	}
	else if (command == "quat") {
		std::string entity;
		float a = 0.0f;
		float b = 0.0f;
		float c = 0.0f;
		float d = 1.0f;
		if (input >> entity >> a >> b >> c >> d) {
			AudioManager_SetQuatValue(entity.c_str(), "rotation", a, b, c, d);
			return true;
		}
	}
	else if (command == "transform") {
		std::string entity;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float a = 0.0f;
		float b = 0.0f;
		float c = 0.0f;
		float d = 1.0f;
		if (input >> entity >> x >> y >> z >> a >> b >> c >> d) {
			AudioManager_SetTransform(entity.c_str(), x, y, z, a, b, c, d);
			return true;
		}
	}

	std::cout << "Invalid command. Type `help` for supported commands.\n";
	return true;
}

} // namespace

int main(int argc, char** argv)
{
	CliOptions options;
	bool shouldExit = false;
	if (!ParseArgs(argc, argv, options, shouldExit)) {
		return shouldExit ? 0 : 1;
	}

	LogMessage("Starting AudioTestCLI", LogCategory::CLI, LogLevel::Info);
	AudioManager_Create(&options.config);

	const std::string assets = options.assetPath.lexically_normal().string();
	const std::string behaviors = options.behaviorPath.lexically_normal().string();
	AudioManager_LoadBehaviorsFromFile(behaviors.c_str(), assets.c_str());
	AudioManager_SetTag("listener", "listener");
	AudioManager_SetVectorValue("listener", "position", 0.0f, 0.0f, 0.0f);

	std::cout << "Assets: " << assets << '\n';
	std::cout << "Behaviors: " << behaviors << '\n';
	PrintPromptHelp();

	std::string line;
	while (std::cout << "> " && std::getline(std::cin, line)) {
		if (!ProcessCommand(line)) {
			break;
		}
	}

	AudioManager_Destroy();
	return 0;
}
