#include <vector>
#include <string>
#include <memory>
#include "Node.hpp"
#include "IBehaviorDefinition.hpp"



class BehaviorDefinitionManager : public IBehaviorDefinition {
public:
	
	// Load or reload a sound-bank. Parses YAML → AST once.
	// creates playdefs and matchdefs?
	void LoadFilesFromFolder(const std::string& path);



	// Inherited via IBehaviorDefinition
	std::vector<PlayDefinition>& GetPlayDefs() override;
	std::vector<MatchDefinition>& GetMatchDefs() override;

	// For hot-reload, Fire an event or return newly loaded defs

private :
	std::vector<PlayDefinition> playdefs;
	std::vector<MatchDefinition> matchdefs;
};