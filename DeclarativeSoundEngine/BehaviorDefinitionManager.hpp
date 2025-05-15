#include <vector>
#include <string>
#include <memory>
#include "Node.hpp"
#include "IBehaviorDefinition.hpp"
#include "BehaviorDef.hpp"


class BehaviorDefinitionManager : public IBehaviorDefinition {
public:
	
	// Load or reload a sound-bank. Parses YAML → AST once.
	// creates playdefs and matchdefs?
	void LoadFilesFromFolder(const std::string& path); // JUST MOVE THIS TO AUDIOMANAGER // TODO


	std::vector<BehaviorDef>& GetBehaviorDefs() { return audioBehaviors; }



	// Inherited via IBehaviorDefinition
	std::vector<PlayDefinition>& GetPlayDefs() override;
	std::vector<MatchDefinition>& GetMatchDefs() override;

	// For hot-reload, Fire an event or return newly loaded defs

private :
	std::vector<PlayDefinition> playdefs;
	std::vector<MatchDefinition> matchdefs;


	std::vector<BehaviorDef> audioBehaviors;
};