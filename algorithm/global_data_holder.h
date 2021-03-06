#pragma once

#include "ldputil.h"
#include "Param.h"
#include <map>
namespace arcsim
{
	class SimulationManager;
}
namespace svg
{
class SvgManager;
};
class CAnalysis2D_Cloth_Static;
class CDesigner2D_Cloth;
class FreeFormDeform;
class GlobalDataHolder
{
public:
	void init();

	void generateClothDebug();

	void svgToCloth();

	void svgToSim();

	void loadLastSvgDir();
	void saveLastSvgDir();
public:
	std::shared_ptr<svg::SvgManager> m_svgManager;
	std::shared_ptr<CAnalysis2D_Cloth_Static> m_clothManger;
	std::shared_ptr<FreeFormDeform> m_svgDefomer;
	std::shared_ptr<CDesigner2D_Cloth> m_clothUiListener;
	std::map<unsigned int, int> m_clothLoopId2svgIdMap;
	std::map<int, unsigned int> m_svgId2clothLoopIdMap;
	Param m_param;


	std::string m_simConfigFileName;
	std::shared_ptr<arcsim::SimulationManager> m_simManager;

	std::string m_lastSvgDir;
};

extern GlobalDataHolder g_dataholder;