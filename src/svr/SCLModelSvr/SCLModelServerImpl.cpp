#include "SCLModelServerImpl.h"
#include "QtCore\QFile.h"
#include "QtCore\QDataStream.h"
#include "SCDLoader.h"
#include "rdb\FacFactory.h"
#include "rdb\SCLModelRdb.h"
#include "SCLModelDefine.h"
#include "LogRecorder.h"
#include "SCLNameTranslator.h"
#include "Spliter.h"
#include "QtCore\QCoreApplication.h"
#include "SCDUnLoader.h"
#include <QDebug>

using namespace std;
//D:/Projects/SSMS/data/PrototypeRdb/
#define strVoltageLevelLibName	"VoltageLevel.db"
#define strSubstationLibName	"Substation.db" //变电站库
#define strPanelLibName			"Panel.db"		//屏柜库
#define strSubNetLibName        "SubNet.db"     //子网库
#define strHisItemLibName       "HItem.db"    //历史记录库
#define strIedLibName			"Ied.db"
#define strNetSwitchLibName		"NetSwitch.db"	//交换机库
#define strPhysicsPortLibName	"PhysicsPort.db" //物理端口库
#define strPhysicsLinkLibName	"PhysicsLink.db" //物理连接库
#define strInSignalLibName		"InSignal.db"	//输入信号库
#define strOutSignalLibName		"OutSignal.db" //输出信号库
#define strSoftTripLibName		"SoftTrip.db" //软压板库
#define strFunctionTripLibName	"FunctionTrip.db" //功能压板库
#define strVirtualLinkLibName	"VirtualLink.db" //内部虚连接库
#define strAPLibName            "AP.db"       //访问点库
#define strCtrlBlockLibName		"CtrlBlock.db"	//控制块库


SCLModelServerImpl::SCLModelServerImpl(void)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		env = rdbFac->createRDBEnvironment();

		QString path;  
		path = QCoreApplication::applicationDirPath();
		path = path + "/../data/PrototypeRdb";
		env->open(path.toUtf8().constData());
	}
	catch(DbException *exc)
	{
		string msg = (string)"模型服务:" + (string)"打开实时库" + string("失败！") + " 原因：" + exc->what();
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
	}
	catch(...)
	{
		string msg = (string)"模型服务:" + (string)"打开实时库" + string("失败！") + " 原因未知";
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
	}
}

SCLModelServerImpl::~SCLModelServerImpl(void)
{
	env->close();
	delete env;
}

QString SCLModelServerImpl::GetSubstationImportTime()
{
	return scdLoader.GetSubstationImportTime();
}

//模型通用接口
::std::string SCLModelServerImpl::GetParentKey(const ::std::string& childKey) //根据子节点Key获得父节点Key
{
    SCLNameTranslator translator;
	return translator.GetParentKey(childKey);
}

::std::string SCLModelServerImpl::GetParentName(const ::std::string& childKey)//根据子节点Key获得父节点的Name
{
	SCLNameTranslator translator;
	return translator.GetParentName(childKey);
}

::std::string SCLModelServerImpl::GetNameFromKey(const ::std::string& key)    //根据Key获得名称
{
	SCLNameTranslator translator;
	return translator.GetNameFromKey(key);
}

::std::string SCLModelServerImpl::GetChildKeyFromParentKeyAndChildName(const ::std::string& parentKey, const ::std::string& childName)//根据父节点的Key和子节点Name获得子节点的Key 
{
	SCLNameTranslator translator;
	return translator.GetChildKeyFromParentKeyAndChildName(parentKey,childName);
}

//模型基本访问接口
bool SCLModelServerImpl::GetVoltageLevelList(::std::vector<::std::string>& nameList)   //获得电压等级名列表（同Key）
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strVoltageLevelLibName);
		if(curRDB->open())
		{
			CRdbVoltageLevel *seria = (CRdbVoltageLevel *)rdbFac->createDBSerialize(VOLTAGELEVEL);
			CRDBCursor *cursor = rdbFac->createRDBCursor();
			bool bFind = curRDB->getFirst(*seria,cursor);

			if(bFind)
			{
				do
				{
					nameList.push_back(seria->name);
				} 
				while(curRDB->getNext(*seria,cursor));
			}
			curRDB->close();
			delete seria;
			delete cursor;
		}

		delete curRDB;
		delete rdbFac;
		return true;	
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得电压等级列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSubstationList(const ::std::string& voltageLevelKey, std::vector<std::string>& subStationKeyList)//获得变电站Key列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);//
		curRDB->setDbName(strVoltageLevelLibName);
		if(curRDB->open())
		{
			CRdbVoltageLevel *seria = (CRdbVoltageLevel *)rdbFac->createDBSerialize(VOLTAGELEVEL);
			curRDB->get(voltageLevelKey.c_str(),*seria);
			for (StrList::iterator  it = seria->subStationList.begin();it != seria->subStationList.end();++ it)
			{
				std::vector<string> keyList = Split(*it,".");
				string subName = keyList.at(1);
				subStationKeyList.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得变电站列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSubstationInfo(const ::std::string& subStationKey, SubStationInfo& subStationInfo)//获得变电站（特定SCD版本）信息	
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		if(curRDB->open())
		{
			CRdbSubstation *seria = (CRdbSubstation *)rdbFac->createDBSerialize(SUBSTATION);
			curRDB->get(subStationKey.c_str(),*seria);
			subStationInfo.Name = seria->name;
			subStationInfo.crc = seria->crc;
			subStationInfo.version = seria->version;
			subStationInfo.reversion = seria->reversion;
			subStationInfo.desc = seria->desc;
			subStationInfo.fileName = seria->fileName;
			subStationInfo.filePath = seria->filePath;
			subStationInfo.toolID = seria->toolID;
			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得变电站信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSCDHItem(const ::std::string& subStationKey, std::vector<HItemInfo>& hItemList)//获得SCD历史修订记录
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strHisItemLibName);
		if(curRDB->open())
		{
			CRdbHItem *seria = (CRdbHItem *)rdbFac->createDBSerialize(HISITEM);
			CRDBCursor *cursor = rdbFac->createRDBCursor();
			bool bFind = curRDB->getFirst(*seria,cursor);

			if(bFind)
			{
				do
				{
					SubStationInfo substationInfo;
					GetSubstationInfo(subStationKey,substationInfo);
					if((substationInfo.Name).compare(seria->substationName) == 0)
					{
						HItemInfo itemInfo;
						itemInfo.version =  seria->version;
						itemInfo.reversion = seria->reversion;
						itemInfo.substationName = seria->substationName;
						itemInfo.what = seria->what;
						itemInfo.when = seria->when;
						itemInfo.who  = seria->who;
						itemInfo.why  = seria->why;
						hItemList.push_back(itemInfo);
					}
				} 
				while(curRDB->getNext(*seria,cursor));
			}
			curRDB->close();
			delete seria;
			delete cursor;
		}

		delete curRDB;
		delete rdbFac;
		return true;	
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得SCD历史修订记录列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSubNetList(const ::std::string& subStationKey, std::vector<std::string>& subNetKeyList)//获得子网Key列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		if(curRDB->open())
		{
			CRdbSubstation *seria = (CRdbSubstation *)rdbFac->createDBSerialize(SUBSTATION);
			curRDB->get(subStationKey.c_str(),*seria);
			for (StrList::iterator  it = seria->subNetList.begin();it != seria->subNetList.end();++ it)
			{
				std::vector<string> keyList = Split(*it,".");
				if (keyList.size() > 1)
				{
					string subNetName = keyList.at(2);
					subNetKeyList.push_back(*it);
				}
			}
			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得子网列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetApInfo(const ::std::string& subNetName,const ::std::string& IEDName, APInfo& apInfo)//获得访问点信息
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strAPLibName);
		if(curRDB->open())
		{
			CRdbAP *seria = (CRdbAP *)rdbFac->createDBSerialize(AP);
			CRDBCursor *cursor = rdbFac->createRDBCursor();
			bool bFind = curRDB->getFirst(*seria,cursor);

			if(bFind)
			{
				do
				{
					if(subNetName==seria->subNetName
						&&IEDName==seria->IEDName)
					{
						apInfo.name = seria->name;
						apInfo.desc = seria->desc;
						apInfo.subNetname = seria->subNetName;
						apInfo.macAddr    = seria->macAddr;
						apInfo.ipAddr     = seria->ipAddr;
					}
				} 
				while(curRDB->getNext(*seria,cursor));
			}
			curRDB->close();
			delete seria;
			delete cursor;
		}

		delete curRDB;
		delete rdbFac;
		return true;	
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得访问点信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}


bool SCLModelServerImpl::GetSubNetInfo(const ::std::string& subNetKey, SubNetInfo& subNetInfo)//获得子网信息
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		if(curRDB->open())
		{
			CRdbSubNet *seria = (CRdbSubNet *)rdbFac->createDBSerialize(SUBNET);
			curRDB->get(subNetKey.c_str(),*seria);
			subNetInfo.name = seria->name;
			subNetInfo.desc = seria->desc;
			subNetInfo.type = seria->type;
			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得变电站信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPanelList(const ::std::string& subStationKey,std::vector<std::string>& subPanelKeyList)//获得屏柜Key列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		if(curRDB->open())
		{
			CRdbSubstation *seria = (CRdbSubstation *)rdbFac->createDBSerialize(SUBSTATION);
			curRDB->get(subStationKey.c_str(),*seria);
			for (StrList::iterator  it = seria->panelList.begin();it != seria->panelList.end();++ it)
			{
				std::vector<string> keyList = Split(*it,".");
				if (keyList.size() > 1)
				{
					string panelName = keyList.at(2);
					subPanelKeyList.push_back(*it);
				}
			}
			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得屏柜列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetIEDList(const ::std::string& panelKey,std::vector<std::string>& subIEDKeyList)//获得装置Key列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPanelLibName);
		if(curRDB->open())
		{
			CRdbPanel *seria = (CRdbPanel *)rdbFac->createDBSerialize(PANEL);
			curRDB->get(panelKey.c_str(),*seria);
			for (StrList::iterator  it = seria->deviceList.begin();it != seria->deviceList.end();++ it)
			{
				std::vector<string> keyList = Split(*it,".");
				string subName = keyList.at(3);
				if (keyList.size() > 2)
				{
					string iedName = keyList.at(3);
					subIEDKeyList.push_back(*it);
				}
			}
			delete seria;
			curRDB->close();
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetIEDListBySubNet(const ::std::string& subNetKey, std::vector<std::string>& subIEDKeyList)//获得装置Key列表(根据子网)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubNetLibName);
		if(curRDB->open())
		{
			CRdbSubNet *seria = (CRdbSubNet *)rdbFac->createDBSerialize(SUBNET);
			curRDB->get(subNetKey.c_str(),*seria);
			for (StrList::iterator  it = seria->deviceList.begin();it != seria->deviceList.end();++ it)
			{
				std::vector<string> keyList = Split(*it,".");
				string subName = keyList.at(3);
				if (keyList.size() > 2)
				{
					string iedName = keyList.at(3);
					subIEDKeyList.push_back(*it);
				}
			}
			delete seria;
			curRDB->close();
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得子网内的装置列表" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetIEDInfo(const ::std::string& IEDKey, DEVInfo& IEDInfo)//获得装置信息
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			IEDInfo.name = seria->name;
			IEDInfo.manu = seria->manu;
			IEDInfo.crc = seria->crc;
			IEDInfo.type = seria->type;
			IEDInfo.configVer = seria->configVer;
			IEDInfo.desc = seria->desc;
			IEDInfo.funVer = seria->funVer;
			IEDInfo.GooseInSigCount = seria->GooseInSigCount;
			IEDInfo.GooseOutSigCount = seria->GooseOutSigCount;
			IEDInfo.SVInSigCount = seria->SVInSigCount;
			IEDInfo.SVOutSigCount = seria->SVOutSigCount;

			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得变电站信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetIEDType(const ::std::string& IEDKey,  int& type)//获得装置类型
{
	bool existFlag = false;

	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			type = IED_TYPE_OTHER;
			existFlag = curRDB->isDataExist(IEDKey.c_str());
			if(existFlag)
			{
				type = IED_TYPE_SWITCH;
			}
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return existFlag;
	}
	catch (...)
	{
		return existFlag;
	}
}

::std::string SCLModelServerImpl::GetSCDVersion(const ::std::string& subStationKey)//获得当前模型的SCD文件版本号
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		SubStationInfo subStationInfo;
		if(curRDB->open())
		{
			CRdbSubstation *seria = (CRdbSubstation *)rdbFac->createDBSerialize(SUBSTATION);
			curRDB->get(subStationKey.c_str(),*seria);
			subStationInfo.Name = seria->name;
			subStationInfo.version = seria->version;
			subStationInfo.reversion = seria->reversion;
			subStationInfo.desc = seria->desc;
			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		if (subStationInfo.version.empty())
		{
			return "未知";
		}
		return subStationInfo.version;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得变电站信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "未知";
	}
}


bool SCLModelServerImpl::IsVoltageLevelExist(const ::std::string& voltageLevelName)//是否存在电压等级
{
	try
	{
		bool existFlag = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strVoltageLevelLibName);
		if(curRDB->open())
		{
			existFlag = curRDB->isDataExist(voltageLevelName.c_str());
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return existFlag;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"判断电压等级是否存在" + string("失败！") + string("电压等级名为：") + voltageLevelName;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::IsSubstationExist(const ::std::string& substationKey)//是否存在变电站
{
	try
	{
		bool existFlag;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSubstationLibName);
		if(curRDB->open())
		{
			existFlag = curRDB->isDataExist(substationKey.c_str());
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return existFlag;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"判断变电站是否存在" + string("失败！") + string("变电站key为：") + substationKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPortsList(const ::std::string& IEDKey,std::vector<std::string>& portKeys)//获得装置的物理端口列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->physicsPortList.begin();it != seria->physicsPortList.end();++ it)
			{
				string portKey = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
				portKeys.push_back(portKey);
			}
			curRDB->close();
			delete seria;
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理端口" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

::std::string SCLModelServerImpl::GetPortDescByPortKey(const ::std::string& portKey)//获得物理端口的描述
{
	try
	{
		string desc = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);
		if(curRDB->open())
		{
			CRdbPhysicsPort *seria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(portKey.c_str(),*seria))
			{
				desc = seria->desc;
			}

			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return desc;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理端口描述" + string("失败！") + string("端口key为：") + portKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "";
	}
}

bool SCLModelServerImpl::GetLinksListOfIED(const ::std::string& IEDKey, std::vector<PhysLinkInfo>& links)//获得装置的物理链接列表
{
	try
	{
		links.clear();
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator it = seria->physicalLink.begin();it != seria->physicalLink.end(); ++it)
			{
				string phylink_keystring = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
				CRdb *phyLinkRDB = rdbFac->createRDB(env);
				phyLinkRDB->setDbName(strPhysicsLinkLibName);
				if(phyLinkRDB->open())
				{
					CRdbPhysicsLink *seriaPLink = (CRdbPhysicsLink *)rdbFac->createDBSerialize(IED);
					bool flag = phyLinkRDB->get(phylink_keystring.c_str(),*seriaPLink);
					if (flag == true)
					{
						for (StrList::iterator j = seriaPLink->destPort.begin();j != seriaPLink->destPort.end(); ++ j)
						{
							PhysLinkInfo slice_p_link;
							slice_p_link.srcPort = seriaPLink->srcPort;
							slice_p_link.destPort = *j;
							links.push_back(slice_p_link);
						}
					}
					delete seriaPLink;
					phyLinkRDB->close();
				}
				delete phyLinkRDB;
			}
			delete seria;
			curRDB->close();
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理连接" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetInSignalsListOfIED(const ::std::string& IEDKey,  SignalTypeEnum signalType,std::vector<std::string>& inSignalKeys)//获得装置的输入信号列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->inSignalList.begin();it != seria->inSignalList.end();++ it)
			{
				string inSignalKey = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
				inSignalKeys.push_back(inSignalKey);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的输入虚信号列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetOutSignalsListOfIED(const ::std::string& IEDKey,  SignalTypeEnum signalType,std::vector<std::string>& outSignalKeys)//获得装置的输出信号列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->outSignalList.begin();it != seria->outSignalList.end();++ it)
			{
				string outSignalKey = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
				outSignalKeys.push_back(outSignalKey);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的输出虚信号列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetVritualLinksOfIED(const ::std::string& IEDKey,  SignalTypeEnum signalType, std::vector<VirtualLinkInfo>& links)//获得装置的虚拟链接列表（装置和装置之间）
{
	try
	{
		links.clear();
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		CRdb *virLinkRDB = rdbFac->createRDB(env);
		virLinkRDB->setDbName(strVirtualLinkLibName);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		outSignalRDB->setDbName(strOutSignalLibName);

		if(curRDB->open()&&
			virLinkRDB->open()&&
			inSignalRDB->open()&&
			outSignalRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator it = seria->virtualLink.begin();it != seria->virtualLink.end(); ++it)
			{
				string virlink_keystring = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);

				CRdbVirtualLink *seriaVLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
				bool flag = virLinkRDB->get(virlink_keystring.c_str(),*seriaVLink);
				if (flag == true)
				{
					StrList::iterator dIt = seriaVLink->destDesc.begin();
					for (StrList::iterator j = seriaVLink->destSignal.begin();j != seriaVLink->destSignal.end(); ++ j)
					//for(int i = 0 ; i != seriaVLink->destSignal.size(); i++)
					{
						VirtualLinkInfo slice_v_link;
						slice_v_link.iOrd = seriaVLink->iOrd;
						CRdbInSignal *seriaInSignal = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
						CRdbOutSignal *seriaOutSignal = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);

						if (seriaVLink->isReverse)
						{
							slice_v_link.srcSignal = *j;
							slice_v_link.destSignal = seriaVLink->srcSignal;
							slice_v_link.inSigDesc = seriaVLink->desc;
						}
						else
						{
							slice_v_link.srcSignal = seriaVLink->srcSignal;
							slice_v_link.destSignal = *j;
							slice_v_link.inSigDesc = *dIt;
						}
						dIt++;

						inSignalRDB->get(slice_v_link.destSignal.c_str(),*seriaInSignal);
						outSignalRDB->get(slice_v_link.srcSignal.c_str(),*seriaOutSignal);

						//-------------------------------------------------------------------------------
						//if(inSignalRDB->get(seriaVLink->srcSignal.c_str(),*seriaInSignal))
						//{
						//	slice_v_link.srcSignal = *j;
						//	slice_v_link.destSignal = seriaVLink->srcSignal;
						//}
						//else
						//{
						//	slice_v_link.srcSignal = seriaVLink->srcSignal;
						//	slice_v_link.destSignal = *j;
						//}
						//outSignalRDB->get(seriaVLink->srcSignal.c_str(),*seriaOutSignal);
						//-------------------------------------------------------------------------------

						if(signalType == typeAll)
						{
							links.push_back(slice_v_link);
						}
						else
						{
							if(signalType == typeGoose && seriaInSignal->type == "GSEControl")
							{
								links.push_back(slice_v_link);
							}
							else if(signalType == typeSV && seriaInSignal->type == "SampledValueControl")
							{
								links.push_back(slice_v_link);
							}
							else if(signalType == typeGoose && seriaOutSignal->type == "GSEControl")
							{
								links.push_back(slice_v_link);
							}
							else if(signalType == typeSV && seriaOutSignal->type == "SampledValueControl")
							{
								links.push_back(slice_v_link);
							}
							
						}
						delete seriaInSignal;
						delete seriaOutSignal;
					}
				}
				delete seriaVLink;
			}
			delete seria;
			virLinkRDB->close();
			inSignalRDB->close();
			outSignalRDB->close();
			curRDB->close();
		}
		delete curRDB;
		delete virLinkRDB;
		delete inSignalRDB;
		delete outSignalRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的虚链接列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetInternalVritualLinksOfIED(const ::std::string& IEDKey,  SignalTypeEnum signalType, std::vector<VirtualLinkInfo>& links)//获得装置内部的虚拟链接列表（装置内部）
{
	string msg = (string)"模型服务:" + (string)"获得装置的内部虚链接列表" + string("失败！") + string("装置key为：") + IEDKey;
	LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
	return false;
}

bool SCLModelServerImpl::GetFunctionsList(const ::std::string& IEDKey,std::vector<std::string>& protectFuncKeys)//获得装置的功能列表
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->funcList.begin();it != seria->funcList.end();++ it)
			{
				string funcKey = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
				protectFuncKeys.push_back(funcKey);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置功能列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetVirtualLinksListOfPhysicsLink(const PhysLinkInfo& phyLink,  SignalTypeEnum signalType, std::vector<VirtualLinkInfo>& links)//获得物理链接关联的虚拟链接列表
{
	try
	{
		CPhysicsLink plink_rec;
		string srcIedKey = GetParentKey(phyLink.srcPort);
		string dstIedKey = GetParentKey(phyLink.destPort);

		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsLinkLibName);
		if(curRDB->open())
		{
			CRdbPhysicsLink *seriaDestIed  = (CRdbPhysicsLink *)rdbFac->createDBSerialize(PHYSICSLINK);
			if(curRDB->get(phyLink.srcPort.c_str(),*seriaDestIed))
			{
				CRdb *phyPortRDB = rdbFac->createRDB(env);
				phyPortRDB->setDbName(strPhysicsPortLibName);
				if(phyPortRDB->open())
				{
					CRdbPhysicsPort *seriaPhyPort = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
					string iedKey = GetParentKey(phyLink.srcPort);
					if(phyPortRDB->get(phyLink.srcPort.c_str(),*seriaPhyPort))
					{
						for (StrList::iterator i = seriaPhyPort->m_VirtualLinks.begin();i != seriaPhyPort->m_VirtualLinks.end();++ i)
						{
							CRdb *virLinkRDB = rdbFac->createRDB(env);
							virLinkRDB->setDbName(strVirtualLinkLibName);
							if(virLinkRDB->open())
							{
								CRdbVirtualLink *seriaVirLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
								string virLinkKey = iedKey + "." + *i;								
								if(virLinkRDB->get(virLinkKey.c_str(),*seriaVirLink))
								{
									for (StrList::iterator j = seriaVirLink->destSignal.begin();j != seriaVirLink->destSignal.end();++j)
									{
										string tmpDestIed = GetIedKeyFromSignal(*j);
										if (tmpDestIed == dstIedKey)
										{
											VirtualLinkInfo alink;
											alink.srcSignal = *i;
											alink.destSignal = *j;
											links.push_back(alink);
										}
									}
									delete seriaVirLink;
									virLinkRDB->close();
								}
								delete virLinkRDB;
							}
						}
					}
					delete seriaPhyPort;
					phyPortRDB->close();
				}
				delete phyPortRDB;
			}
			delete seriaDestIed;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得物理链接的虚链接列表" + string("失败！")
			+ string("物理链接源端口为：") + phyLink.srcPort 
			+ string(" 物理链接目的端口为：") + phyLink.destPort;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetTripsList(const ::std::string& IEDKey,std::vector<std::string>& tripKeys )//获得装置的压板列表  
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(IEDKey.c_str(),*seria))
			{
				for (StrList::iterator it = seria->softTripList.begin();it != seria->softTripList.end();++it)
				{
					string tripKey = GetChildKeyFromParentKeyAndChildName(IEDKey,*it);
					tripKeys.push_back(tripKey);
				}
			}
			delete seria;
			curRDB->close();
		}

		//交换机还需要完善压板
		//CRdb *curRDBSwitch = rdbFac->createRDB(env);
		//curRDBSwitch->setDbName(strNetSwitchLibName);
		//if(curRDBSwitch->open())
		//{
		//	CRdbNetSwitch *seriaNetSwitch = (CRdbNetSwitch *)rdbFac->createDBSerialize(NETSWITCH);
		//	curRDB->get(IEDKey.c_str(),*seriaNetSwitch);
		//	delete seriaNetSwitch;
		//	curRDBSwitch->close();
		//}
		delete curRDB;
		//delete curRDBSwitch;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的压板列表" + string("失败！")
			+ string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetInfoOfInSignal(const ::std::string& signalKey, SignalInfo& signalInfo)//获得输入信号的信息
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strInSignalLibName);
		bool bFindFlag = false;
		if(curRDB->open())
		{
			CRdbInSignal *seria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			if(curRDB->get(signalKey.c_str(),*seria))
			{
				signalInfo.iOrd		  = seria->iOrd;
				signalInfo.iOutSigOrd = seria->iOutSigOrd;
				signalInfo.signalName = seria->name;
				signalInfo.signalDesc = seria->desc;
				signalInfo.signalPath = seria->path;
				signalInfo.BDAType = seria->BDAType;
				signalInfo.dataSet = seria->dataSet;
				signalInfo.DOIDesc = seria->DOIDesc;
				signalInfo.dUVal = seria->dUVal;
				signalInfo.fc = seria->fc;
				signalInfo.iedName = seria->iedName;
				signalInfo.LnName = seria->LnName;
				signalInfo.LnDesc = seria->LnDesc;
				signalInfo.plugType = seria->plugType;
//				signalInfo.portIndex = seria->portIndex;
				signalInfo.softTripKey = seria->softTripKey;
				signalInfo.state = seria->state;
				if(seria->type == "GSEControl")
				{
					signalInfo.signalType = typeGoose;
				}
				else
				{
					signalInfo.signalType = typeSV;
				}

				bFindFlag = true;
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return bFindFlag;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得输入虚信号的信息" + string("失败！")
			+ string("信号key为：") + signalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}
bool SCLModelServerImpl::GetInfoOfOutSignal(const ::std::string& signalKey, SignalInfo& signalInfo)//获得输出信号的信息 
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strOutSignalLibName);
		bool bFindFlag = false;
		if(curRDB->open())
		{
			CRdbOutSignal *seria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
			if(curRDB->get(signalKey.c_str(),*seria))
			{
				signalInfo.iOrd		  = seria->iOrd;
				signalInfo.iOutSigOrd = seria->iOutSigOrd;
				signalInfo.signalName = seria->name;
				signalInfo.signalDesc = seria->desc;
				signalInfo.signalPath = seria->path;
				signalInfo.BDAType = seria->BDAType;
				signalInfo.dataSet = seria->dataSet;
				signalInfo.DOIDesc = seria->DOIDesc;
				signalInfo.dUVal = seria->dUVal;
				signalInfo.fc = seria->fc;
				signalInfo.iedName = seria->iedName;
				signalInfo.LnName = seria->LnName;
				signalInfo.LnDesc = seria->LnDesc;
				signalInfo.plugType = seria->plugType;
//				signalInfo.portIndex = seria->portIndex;
				signalInfo.softTripKey = seria->softTripKey;
				signalInfo.state = seria->state;
				if(seria->type == "GSEControl")
				{
					signalInfo.signalType = typeGoose;
				}
				else
				{
					signalInfo.signalType = typeSV;
				}
				signalInfo.ctrlBlockID = seria->ctrlBlockID;
				bFindFlag = true;
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return bFindFlag;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得输出虚信号的信息" + string("失败！")
			+ string("信号key为：") + signalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

//高级访问接口
bool SCLModelServerImpl::GetIEDListByLogicalRealitionWithCurrentIED(const ::std::string& IEDKey,std::vector<std::string>& associatedIEDKeys)//获得IED的关联IED列表（逻辑）
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seriaIed  = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(IEDKey.c_str(),*seriaIed))
			{
				for (StrList::iterator  it = seriaIed->virtualLink.begin();it != seriaIed->virtualLink.end();++ it)
				{
					CRdb *virLinkRDB = rdbFac->createRDB(env);
					virLinkRDB->setDbName(strVirtualLinkLibName);
					if(virLinkRDB->open())
					{
						CRdbVirtualLink *seriaVirLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
						string virLinkKey = IEDKey + "." + *it;	
						if(virLinkRDB->get(virLinkKey.c_str(),*seriaVirLink))
						{
							std::vector<string> keyList = Split(seriaVirLink->srcSignal,".");
							for (StrList::iterator j = seriaVirLink->destSignal.begin();j != seriaVirLink->destSignal.end(); ++ j)
							{
								std::vector<string> keyList2 = Split(*j,".");
								string destIedKey = keyList2[0] + "." + keyList2[1] + "." + keyList2[2] + "." + keyList2[3];
								bool founded = false;
								for (vector<string>::iterator it2 = associatedIEDKeys.begin();it2 != associatedIEDKeys.end(); ++it2)
								{
									if (destIedKey == *it2)
									{
										founded = true;
										break;
									}
								}
								if (!founded && keyList2[3]!="")
								{
									associatedIEDKeys.push_back(destIedKey);
								}
							}
						}
						delete seriaVirLink;
						virLinkRDB->close();
					}
					delete virLinkRDB;
				}
			}
			delete seriaIed;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得与当前装置有逻辑关联的装置列表" + string("失败！")
			+ string("当前装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

//用以下四个值表示主从设备之间的逻辑关联类型
// -1，表示查询失败
// 0，表示二者没关系
// 1，表示对主来说的输入
// 2，表示对主来说的输出
// 3，表示输入和输出
bool SCLModelServerImpl::GetRealitionTypeOfSrcIEDAndDestIED(const ::std::string& srcIEDKey, const ::std::string& destIEDKey, int& type)//获得IED之间的关联类型（0：源IED为输入）
{
	try
	{
		bool bFoundInSignal = false;
		bool bFoundOutSignal = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *iedRDB = rdbFac->createRDB(env);
		iedRDB->setDbName(strIedLibName);
		CRdb *virLinkRDB = rdbFac->createRDB(env);
		virLinkRDB->setDbName(strVirtualLinkLibName);

		if(iedRDB->open()&&
			virLinkRDB->open())
		{
			CRdbIed *seriaIed  = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(iedRDB->get(srcIEDKey.c_str(),*seriaIed))
			{
				for (StrList::iterator  it = seriaIed->virtualLink.begin();it != seriaIed->virtualLink.end();++ it)
				{
					CRdbVirtualLink *seriaVirLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
					string virlink_key_string = srcIEDKey + "." + *it;
					if(virLinkRDB->get(virlink_key_string.c_str(),*seriaVirLink))
					{
						for (StrList::iterator j = seriaVirLink->destSignal.begin();j != seriaVirLink->destSignal.end();++j)
						{
							std::vector<string> keyList2 = Split(*j,".");
							string destIedKey = keyList2[0] + "." + keyList2[1] + "." + keyList2[2] + "." + keyList2[3];
							if (destIedKey == destIEDKey)
							{
								//判断virlink_key_string，与ied_rec.inSignalList和 ied_rec.outSignalList逐条比对
								for (StrList::iterator m = seriaIed->inSignalList.begin(); m != seriaIed->inSignalList.end(); ++m)
								{
									string insign_key_str = srcIEDKey + "." + *m;
									if (virlink_key_string == insign_key_str)
									{
										bFoundInSignal = true;
									}
								}
								for (StrList::iterator n = seriaIed->outSignalList.begin();n != seriaIed->outSignalList.end(); ++n)
								{
									string insign_key_str = srcIEDKey + "." + *n;
									if (virlink_key_string == insign_key_str)
									{
										bFoundOutSignal = true;
									}
								}
							}
						}
					}
					delete seriaVirLink;
				}
			}
			delete seriaIed;
			iedRDB->close();
			virLinkRDB->close();
			delete iedRDB;
			delete virLinkRDB;
		}

		int result = (((int)bFoundOutSignal) << 1) + ((int)bFoundInSignal);
		type = result;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得源装置与目的装置之间的关联类型" + string("失败！")
			+ string("当前装置key为：") + srcIEDKey + string(" 目的装置key为：") + destIEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}


bool SCLModelServerImpl::GetIEDListByPhysicalRealitionWithCurrentIED(const ::std::string& IEDKey,std::vector<std::string>& associatedIEDKeys)//获得IED的关联IED列表（物理） 
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seriaIed  = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(IEDKey.c_str(),*seriaIed))
			{
				for (StrList::iterator  it = seriaIed->physicalLink.begin();it != seriaIed->physicalLink.end();++ it)
				{
					CRdb *phyLinkRDB = rdbFac->createRDB(env);
					phyLinkRDB->setDbName(strPhysicsLinkLibName);
					if(phyLinkRDB->open())
					{
						CRdbPhysicsLink *seriaPhyLink = (CRdbPhysicsLink *)rdbFac->createDBSerialize(PHYSICSLINK);
						string phyLinkKey = IEDKey + "." + *it;	
						if(phyLinkRDB->get(phyLinkKey.c_str(),*seriaPhyLink))
						{
							std::vector<string> keyList = Split(seriaPhyLink->srcPort,".");
							for (StrList::iterator j = seriaPhyLink->destPort.begin();j != seriaPhyLink->destPort.end(); ++ j)
							{
								std::vector<string> keyList2 = Split(*j,".");
								string destIedKey = keyList2[0] + "." + keyList2[1] + "." + keyList2[2] + "." + keyList2[3];
								bool founded = false;
								for (vector<string>::iterator it2 = associatedIEDKeys.begin();it2 != associatedIEDKeys.end(); ++it2)
								{
									if (destIedKey == *it2)
									{
										founded = true;
										break;
									}
								}
								if (!founded)
								{
									associatedIEDKeys.push_back(destIedKey);
								}
							}
						}
						delete seriaPhyLink;
						phyLinkRDB->close();
					}
					delete phyLinkRDB;
				}
			}
			delete seriaIed;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得与当前装置有物理链接关系的装置列表" + string("失败！")
			+ string("当前装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}
 
bool SCLModelServerImpl::GetPhysicsLinkAssociatedIED(const ::std::string& srcIEDKey, const ::std::string& destIEDKey, std::vector<PhysLinkInfo>& links)//获得IED与IED之间物理链接（多个链接表示多个物理通道，0个链接表示无直接连接）
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seriaIed  = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(srcIEDKey.c_str(),*seriaIed))
			{
				for (StrList::iterator  it = seriaIed->physicalLink.begin();it != seriaIed->physicalLink.end();++ it)
				{
					CRdb *phyLinkRDB = rdbFac->createRDB(env);
					phyLinkRDB->setDbName(strPhysicsLinkLibName);
					if(phyLinkRDB->open())
					{
						CRdbPhysicsLink *seriaPhyLink = (CRdbPhysicsLink *)rdbFac->createDBSerialize(PHYSICSLINK);
						string phyLinkKey = *it;	
						if(phyLinkRDB->get(phyLinkKey.c_str(),*seriaPhyLink))
						{
							std::vector<string> keyList = Split(seriaPhyLink->srcPort,".");
							string tmpIedKey = keyList[0] + "." + keyList[1] + "." + keyList[2] + "." + keyList[3];
							for (StrList::iterator j = seriaPhyLink->destPort.begin();j != seriaPhyLink->destPort.end(); ++ j)
							{
								std::vector<string> keyList2 = Split(*j,".");
								string destIedKey = keyList2[0] + "." + keyList2[1] + "." + keyList2[2] + "." + keyList2[3];
								bool founded = false;
				            }
						}
						delete seriaPhyLink;
						phyLinkRDB->close();
					}
					delete phyLinkRDB;
				}
			}
			delete seriaIed;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得源装置与目的装置的物理链接列表" + string("失败！")
			+ string("源装置key为：") + srcIEDKey
			+ string(" 目的装置key为：") + destIEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

//这两个物理端口的物理连接存在多个吗？？？有问题，需重写
bool SCLModelServerImpl::GetLinksBetweenPort(const ::std::string& srcPortKey, const ::std::string& destPortKey, vector<PhysLinkInfo>& links)//获得原port与目的port之间的物理链接（多个链接表示之间有多个设备）
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);
		if(curRDB->open())
		{
			CRdbPhysicsPort *seriaPort  = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
			if(curRDB->get(srcPortKey.c_str(),*seriaPort))
			{
				for (StrList::iterator  it = seriaPort->m_PhysicsLinks.begin();it != seriaPort->m_PhysicsLinks.end();++ it)
				{
					CRdb *phyLinkRDB = rdbFac->createRDB(env);
					phyLinkRDB->setDbName(strPhysicsLinkLibName);
					if(phyLinkRDB->open())
					{
						CRdbPhysicsLink *seriaPhyLink = (CRdbPhysicsLink *)rdbFac->createDBSerialize(PHYSICSLINK);
						string phyLinkKey = *it;	
						if(phyLinkRDB->get(phyLinkKey.c_str(),*seriaPhyLink))
						{
							for (StrList::iterator j = seriaPhyLink->destPort.begin();j != seriaPhyLink->destPort.end(); ++ j)
							{
								if (destPortKey == *j)
								{
									PhysLinkInfo slice_p_link;
									slice_p_link.srcPort = seriaPhyLink->srcPort;
									slice_p_link.destPort = *j;
									links.push_back(slice_p_link);
								}
							}
						}
						delete seriaPhyLink;
						phyLinkRDB->close();
					}
					delete phyLinkRDB;
				}
			}
			delete seriaPort;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得源端口与目的端口的虚链接列表" + string("失败！")
			+ string("源端口key为：") + srcPortKey
			+ string(" 目的端口key为：") + destPortKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

/*
bool SCLModelServerImpl::GetVirtualSignalsAssociatedIED(const ::std::string& srcIEDKey, const ::std::string& destIEDKey,  ::SCLModel::SignalTypeEnum signalType, ::SCLModel::VirtualLinkSeq& links)//获得IED与IED之间虚信号 
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *iedRDB = rdbFac->createRDB(env);
		iedRDB->setDbName(strIedLibName);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);
		CRdb *virLinkRDB = rdbFac->createRDB(env);
		virLinkRDB->setDbName(strVirtualLinkLibName);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		outSignalRDB->setDbName(strOutSignalLibName);

		if(iedRDB->open()&&
			inSignalRDB->open()&&
			virLinkRDB->open()&&
			outSignalRDB->open())
		{
			CRdbIed *seriaIed  = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(iedRDB->get(srcIEDKey.c_str(),*seriaIed))
			{
				for (StrList::iterator  it = seriaIed->inSignalList.begin();it != seriaIed->inSignalList.end();++ it)
				{
					CRdbInSignal *seriaInSignal = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
					string inSignalKey = GetChildKeyFromParentKeyAndChildName(srcIEDKey,*it);	
					if(inSignalRDB->get(inSignalKey.c_str(),*seriaInSignal))
					{
						CRdbVirtualLink *seriaVirLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
						if(virLinkRDB->get(inSignalKey.c_str(),*seriaVirLink))
						{
							for (StrList::iterator j = seriaVirLink->destSignal.begin();j != seriaVirLink->destSignal.end();++j)
							{
								//打开OutSignal定位
								CRdbOutSignal *seriaOutSignal = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
								if(outSignalRDB->get((*j).c_str(),*seriaOutSignal))
								{
									string strDestSignalPort = *j;
									std::vector<string> keyList = Split(strDestSignalPort,".");
									string dest_ied_key = keyList[0] + "." + keyList[1] + "." + keyList[2] + "." + keyList[3];
									if (dest_ied_key == destIEDKey)
									{
										SliceVirtualLink alink;
										alink.srcSignal = seriaVirLink->srcSignal;
										alink.destSignal = *j;
										links.push_back(alink);
										break;
									}
								}
								delete seriaOutSignal;
							}
						}
			            delete seriaVirLink;
					}
					delete seriaInSignal;

					for (StrList::iterator  it1 = seriaIed->outSignalList.begin();it1 != seriaIed->outSignalList.end();++ it1)
					{
						CRdbOutSignal *seriaOutSignal = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
						string outSignalKey = GetChildKeyFromParentKeyAndChildName(srcIEDKey,*it1);	

						if(outSignalRDB->get(outSignalKey.c_str(),*seriaOutSignal))
						{
							CRdbVirtualLink *seriaVirLink = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
							if(virLinkRDB->get(outSignalKey.c_str(),*seriaVirLink))
							{
								for (StrList::iterator j = seriaVirLink->destSignal.begin();j != seriaVirLink->destSignal.end();++j)
								{
									//打开InSignal定位
									CRdbInSignal *tempSeriaInSignal = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
									if(inSignalRDB->get((*j).c_str(),*tempSeriaInSignal))
									{
										string strDestSignalPort = *j;
										std::vector<string> keyList = Split(strDestSignalPort,".");
										string iedName = keyList.at(3);

										string dest_ied_key = keyList[0] + "." + keyList[1] + "." + keyList[2] + "." + keyList[3];
										if (dest_ied_key == destIEDKey)
										{
											SliceVirtualLink alink;
											alink.srcSignal = seriaVirLink->srcSignal;
											alink.destSignal = *j;
											links.push_back(alink);
											break;
										}
									}
									delete seriaOutSignal;
								}
							}
							delete seriaVirLink;
						}
					}
				}
			}
			delete seriaIed;
			iedRDB->close();
			inSignalRDB->close();
			virLinkRDB->close();
			outSignalRDB->close();

			delete iedRDB;
			delete inSignalRDB;
			delete virLinkRDB;
			delete outSignalRDB;
		}

	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得源装置与目的装置的虚链接列表" + string("失败！")
			+ string("源装置key为：") + srcIEDKey
			+ string(" 目的装置key为：") + destIEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}*/

bool SCLModelServerImpl::GetVirtualSignalsAssociatedIED(const ::std::string& srcIEDKey, const ::std::string& destIEDKey,  SignalTypeEnum signalType, std::vector<VirtualLinkInfo>& links)//获得IED与IED之间虚信号 
{
	try
	{
		DEVInfo destInfo;
		GetIEDInfo(destIEDKey,destInfo);
		if (destInfo.name.empty())
		{
			return false;
		}
		std::vector<VirtualLinkInfo> tempLinks;
		GetVritualLinksOfIED(srcIEDKey,signalType,tempLinks);
		for(uint i=0;i<tempLinks.size();i++)
		{
			string strSrcIEDKey  = GetParentKey(tempLinks[i].srcSignal);
			string strDestIEDKey = GetParentKey(tempLinks[i].destSignal);
			if((strSrcIEDKey==srcIEDKey&&strDestIEDKey==destIEDKey)||
				(strSrcIEDKey==destIEDKey&&strDestIEDKey==srcIEDKey))
			{
				links.push_back(tempLinks[i]);
			}
		}
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得源装置与目的装置的虚链接列表" + string("失败！")
			+ string("源装置key为：") + srcIEDKey
			+ string(" 目的装置key为：") + destIEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPortsListOfPort(const ::std::string& portKey,std::vector<std::string>& portKeys)//获得与物理端口关联的其它物理端口
{
	return false;
}

bool SCLModelServerImpl::GetPhysicsLinkeByPort(const ::std::string& srcPortKey, const ::std::string& destPortKey, PhysLinkInfo& link)//根据两个端口获得物理链接 
{
	return false;
}

bool SCLModelServerImpl::GetInSignalsListOfOutSignal(const ::std::string& outSignalKey,std::vector<std::string>& inSignalKeys)//获得输出信号关联的输入信号列表（两个装置之间）
{
	CFacFactory fac;
	CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
	CRdb *virtualLinkRDB = rdbFac->createRDB(env);
	virtualLinkRDB->setDbName(strVirtualLinkLibName);
	virtualLinkRDB->open();

	CRdbVirtualLink *seria = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
	bool bFind = virtualLinkRDB->get(outSignalKey.c_str(),*seria);
	if(bFind)
	{
		for (StrList::iterator  it = seria->destSignal.begin();it != seria->destSignal.end();++ it)
		{
			inSignalKeys.push_back(*it);
		}
	}
	virtualLinkRDB->close();

	delete seria;
	delete virtualLinkRDB;
	delete rdbFac;
	return true;
}

bool SCLModelServerImpl::GetOutSignalsListOfInSignal(const ::std::string& inSignalKey,std::vector<std::string>& outSignalKeys)//获得输入信号关联的输出信号列表（两个装置之间）
{
	CFacFactory fac;
	CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
	CRdb *virtualLinkRDB = rdbFac->createRDB(env);
	virtualLinkRDB->setDbName(strVirtualLinkLibName);
	virtualLinkRDB->open();

	CRdbVirtualLink *seria = (CRdbVirtualLink *)rdbFac->createDBSerialize(VIRTUALLINK);
	bool bFind = virtualLinkRDB->get(inSignalKey.c_str(),*seria);
	if(bFind)
	{
		for (StrList::iterator  it = seria->destSignal.begin();it != seria->destSignal.end();++ it)
		{
			outSignalKeys.push_back(*it);
		}
	}
	virtualLinkRDB->close();

	delete seria;
	delete virtualLinkRDB;
	delete rdbFac;
	return true;
}

bool SCLModelServerImpl::GetInternalOutSignalsListOfInSignal(const ::std::string& inSignalKey,std::vector<std::string>& outSignalKeys)//获得输入信号关联的输出信号列表（装置内部）
{
	return false;
}

bool SCLModelServerImpl::GetInternalInSignalsListOfOutSignal(const ::std::string& outSignalKey,std::vector<std::string>& inSignalKeys)//获得输出信号关联的输入信号列表（装置内部）
{
	return false;
}
bool SCLModelServerImpl::GetVirtualLinkBySignal(const ::std::string& srcSignalKey, const ::std::string& destSignalKey, VirtualLinkInfo& link)//根据两个信号获得虚拟链接 
{
	return false;
}

bool SCLModelServerImpl::GetFunctionListOfInSignal(const ::std::string& inSignalKey,std::vector<std::string>& funcKeys)//获得输入信号关联的功能列表（本装置功能）
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strInSignalLibName);
		if(curRDB->open())
		{
			CRdbInSignal *seria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			curRDB->get(inSignalKey.c_str(),*seria);
			for (StrList::iterator  i = seria->funcList.begin();i != seria->funcList.end(); ++i)
			{
				funcKeys.push_back(*i);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得输入信号的功能列表" + string("失败！")
			+ string("输入信号的key为：") + inSignalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetFunctionsListOfOutSignal(const ::std::string& outSignalKey,std::vector<std::string>& funcKeys)//获得输出信号关联的功能列表 （其它装置功能）
{
	return false;
}

bool SCLModelServerImpl::GetFunctionsListOfOutSignalWithDestIED(const ::std::string& outSignalKey, const ::std::string& destIEDKey,std::vector<std::string>& funcKeys)//获得输出信号相对于某装置的功能列表（某装置功能）
{
	return false;
}

bool SCLModelServerImpl::GetFunctionsListOfPort(const ::std::string& portKey,std::vector<std::string>& FuncKeys)//获得物理端口功能列表
{
	return false;
}

bool SCLModelServerImpl::GetFunctionsListOfTrip(const ::std::string& tripKey,std::vector<std::string>& FuncKeys)//获得压板对应的功能列表
{
	string signalKey;
	if(GetSignalKeyOfTrip(tripKey,signalKey))
	{
		vector<string> inSignalKeys;
		if(GetInSignalsListOfOutSignal(signalKey,inSignalKeys))
		{
			for (int i=0;inSignalKeys.size();i++)
			{
				if(GetFunctionListOfInSignal(inSignalKeys.at(i),FuncKeys))
				{
					return true;
				}
			}
		}
	}
	return false;
}

::std::string SCLModelServerImpl::GetFunctionDesc(const ::std::string& functionKey)//获得功能的功能描述
{
	try
	{
		string desc = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strFunctionTripLibName);
		if(curRDB->open())
		{
			CRdbFunction *seria = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
			if(curRDB->get(functionKey.c_str(),*seria))
			{
				desc = seria->desc;
			}	
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return desc;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得功能描述" + string("失败！")
			+ string("功能的key为：") + functionKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "";
	}
}

::std::string  SCLModelServerImpl::GetFunctionTripKey(const ::std::string& functionKey)//获得功能的功能压板（可能不存在）
{
	try
	{
		string tripKey = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strFunctionTripLibName);
		if(curRDB->open())
		{
			CRdbFunction *seria = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
			if(curRDB->get(functionKey.c_str(),*seria))
			{
				tripKey = seria->m_SoftTrip;
			}	
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return tripKey;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得功能压板" + string("失败！")
			+ string("功能的key为：") + functionKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "";
	}
}

::std::string  SCLModelServerImpl::GetIEDTripKey(const ::std::string& IEDKey )//获得设备的压板（表示断电一类的操作）
{
	return "";
}

::std::string  SCLModelServerImpl::GetPortTripKey(const ::std::string& portKey )//获得端口的压板（表示拨光纤一类的操作）
{
	try
	{
		string tripKey = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);
		if(curRDB->open())
		{
			CRdbPhysicsPort *seria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
			if(curRDB->get(portKey.c_str(),*seria))
			{
				tripKey = seria->tripKey;
			}	
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return tripKey;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得物理端口压板" + string("失败！")
			+ string("端口的key为：") + portKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "";
	}
}

::std::string  SCLModelServerImpl::GetSignalTripKey(const ::std::string& signalKey )//获得信号的压板（软压板）
{
	try
	{
		string tripKey = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		outSignalRDB->setDbName(strOutSignalLibName);
		if(inSignalRDB->open())
		{
			CRdbInSignal *seria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			if(inSignalRDB->get(signalKey.c_str(),*seria))
			{
				tripKey = seria->softTripKey;
			}	
			delete seria;
			inSignalRDB->close();
		}

		if(tripKey=="")
		{
			if(outSignalRDB->open())
			{
				CRdbOutSignal *seria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
				if(outSignalRDB->get(signalKey.c_str(),*seria))
				{
					tripKey = seria->softTripKey;
				}	
				delete seria;
				outSignalRDB->close();
			}
		}
		delete inSignalRDB;
		delete outSignalRDB;
		delete rdbFac;
		return tripKey;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得虚信号的压板" + string("失败！")
			+ string("信号的key为：") + signalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return "";
	}
}

bool SCLModelServerImpl::GetCtrlBlockListByIED(const ::std::string& IEDKey, std::vector<std::string>& ctrlBlockKeys)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->ctrlBlockList.begin();it != seria->ctrlBlockList.end();++ it)
			{
				//CtrlBlockInfo cbInfo;
				//GetCtrlBlockInfo(*it,cbInfo);
				//if (cbInfo.type.compare("ReportControl") == 0)
				//{
				//	continue;
				//}
				ctrlBlockKeys.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的控制块列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetCtrlBlockInfo(const ::std::string& ctrlBlockKey, CtrlBlockInfo& cbInfo)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strCtrlBlockLibName);
		if(curRDB->open())
		{
			CRdbCtrlBlock *seria = (CRdbCtrlBlock *)rdbFac->createDBSerialize(CTRLBLOCK);
			curRDB->get(ctrlBlockKey.c_str(),*seria);
			cbInfo.name = seria->name;
			cbInfo.type = seria->type;
			cbInfo.apAPPID = seria->apAPPID;
			cbInfo.apMAC = seria->apMAC;
			cbInfo.apName = seria->apName;
			cbInfo.APPID = seria->APPID;
			cbInfo.apVLAN_ID = seria->apVLAN_ID;
			cbInfo.apVLAN_PRIORITY = seria->apVLAN_PRIORITY;
			cbInfo.dataSetName = seria->dataSetName;
			cbInfo.dataSetAddr = seria->dataSetAddr;
			cbInfo.confRev = seria->confRev;
			cbInfo.ASDU = seria->ASDU;
			cbInfo.gocbRef = seria->gocbRef;
			cbInfo.smpRate = seria->smpRate;
			cbInfo.maxTime = seria->maxTime;
			cbInfo.minTime = seria->minTime;

			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得控制块信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetDataSetSignalsByCtrlBlockID(const ::std::string& cbKey, std::vector<std::string>& dsSignalList)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strCtrlBlockLibName);
		if(curRDB->open())
		{
			CRdbCtrlBlock *seria = (CRdbCtrlBlock *)rdbFac->createDBSerialize(CTRLBLOCK);
			curRDB->get(cbKey.c_str(),*seria);
			for (StrList::iterator  it = seria->dataSetSignalList.begin();it != seria->dataSetSignalList.end();++ it)
			{
				dsSignalList.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得控制块的信号列表" + string("失败！") + string("装置key为：") + cbKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSlaveCtrlBlockByIEDKeys(const ::std::string& masterKey, const ::std::string& slaveKey, std::vector<std::string>& ctrlBlockList)
{
	std::vector<std::string> allCBList;
	GetCtrlBlockListByIED(slaveKey, allCBList);
	bool isRecv = false;
	for (int i = 0; i != allCBList.size(); i++)
	{
		bool isInclude = false;
		string cbKey = allCBList.at(i);
		CtrlBlockInfo cbInfo;
		GetCtrlBlockInfo(cbKey,cbInfo);
		if (cbInfo.type.compare("ReportControl")==0)
		{
			continue;
		}

		vector<string> assoIEDs;
		GetAssoIEDListByCBKey(cbKey, assoIEDs);
		for (int j = 0; j != assoIEDs.size(); j++)
		{
			if (masterKey.compare(assoIEDs.at(j)) == 0)
			{
				isInclude = true;
				break;
			}
		}
				
		if (isInclude)
		{
			ctrlBlockList.push_back(cbKey);
		}
	}

	isRecv = IsRecv(masterKey, slaveKey);
	if (isRecv)
	{
		ctrlBlockList.push_back("接收");
	}
	return true;
	//-------------------------------旧逻辑--------------------------------------;
	//std::vector<std::string> allCBList;
	//std::vector<VirtualLinkInfo> vLinks;
	//GetCtrlBlockListByIED(slaveKey, allCBList);
	//GetVirtualSignalsAssociatedIED(masterKey,slaveKey,SignalTypeEnum::typeAll, vLinks);
	//bool isRecv = false;
	//for (int i = 0; i != allCBList.size(); i++)
	//{
	//	bool isInclude = false;
	//	string cbKey = allCBList.at(i);
	//	CtrlBlockInfo cbInfo;
	//	GetCtrlBlockInfo(cbKey,cbInfo);
	//	if (cbInfo.type.compare("ReportControl")==0)
	//	{
	//		continue;
	//	}

	//	vector<string> cbSigs;
	//	GetOutSignalsByCBKey(slaveKey, cbKey, cbSigs);
	//	for (int j = 0; j != cbSigs.size(); j++)
	//	{
	//		for (int k = 0; k != vLinks.size(); k++)
	//		{
	//			VirtualLinkInfo vLink = vLinks.at(k);
	//			if (vLink.srcSignal.compare(cbSigs.at(j)) == 0)
	//			{
	//				isInclude = true;
	//				break;
	//			}
	//			if (GetParentKey(vLink.destSignal).compare(slaveKey) == 0)
	//			{
	//				isRecv = true;
	//			}
	//		}
	//		if (isInclude)
	//		{
	//			break;
	//		}
	//	}
	//	if (isInclude)
	//	{
	//		ctrlBlockList.push_back(cbKey);
	//	}
	//}

	//if (isRecv)
	//{
	//	ctrlBlockList.push_back("接收");
	//}
	//return true;
}

bool SCLModelServerImpl::IsRecv(const std::string& masterKey, const std::string& slaveKey)
{
	std::vector<std::string> allCBList;
	GetCtrlBlockListByIED(masterKey, allCBList);
	bool isRecv = false;
	for (int i = 0; i != allCBList.size(); i++)
	{
		bool isInclude = false;
		string cbKey = allCBList.at(i);
		CtrlBlockInfo cbInfo;
		GetCtrlBlockInfo(cbKey,cbInfo);
		if (cbInfo.type.compare("ReportControl")==0)
		{
			continue;
		}

		vector<string> assoIEDs;
		GetAssoIEDListByCBKey(cbKey, assoIEDs);
		for (int j = 0; j != assoIEDs.size(); j++)
		{
			if (slaveKey.compare(assoIEDs.at(j)) == 0)
			{
				isInclude = true;
				break;
			}
		}

		if (isInclude)
		{
			isRecv = true;
			break;
		}
	}
	return isRecv;
}

bool SCLModelServerImpl::GetOutSignalsByCBKey(const ::std::string& masterIEDKey, const ::std::string& cbKey, std::vector<std::string>& outSignals)
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strCtrlBlockLibName);
		if(curRDB->open())
		{
			CRdbCtrlBlock *seria = (CRdbCtrlBlock *)rdbFac->createDBSerialize(CTRLBLOCK);
			curRDB->get(cbKey.c_str(),*seria);
			for (StrList::iterator  it = seria->dataSetSignalList.begin();it != seria->dataSetSignalList.end();++ it)
			{
				outSignals.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得控制块的信号列表" + string("失败！") + string("装置key为：") + cbKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetAssoIEDListByCBKey(const ::std::string& cbKey, std::vector<std::string>& assoIEDKeys)		//通过控制块获取对端IED;
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strCtrlBlockLibName);
		if(curRDB->open())
		{
			CRdbCtrlBlock *seria = (CRdbCtrlBlock *)rdbFac->createDBSerialize(CTRLBLOCK);
			curRDB->get(cbKey.c_str(),*seria);
			for (StrList::iterator  it = seria->assoIEDKey.begin();it != seria->assoIEDKey.end();++ it)
			{
				assoIEDKeys.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得控制块的对端IED列表" + string("失败！") + string("装置key为：") + cbKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPhyPortInfo(const ::std::string& pPortKey, PhyPortInfo& pPortInfo)//获取物理端口信息;
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);
		if(curRDB->open())
		{
			CRdbPhysicsPort *seria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
			curRDB->get(pPortKey.c_str(),*seria);
//			pPortInfo.ID = seria->ID;
//			pPortInfo.pIndex = seria->pIndex;
//			pPortInfo.desc = seria->desc;
			pPortInfo.plug = seria->plug;
//			pPortInfo.type = seria->type;
//			pPortInfo.rtType = seria->rtType;

			curRDB->close();
			delete seria;
		}

		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得物理端口信息" + string("失败！");
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPhyPortListByIED(const ::std::string& IEDKey, std::vector<std::string>& pPortList)//根据IED获取物理端口;
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			curRDB->get(IEDKey.c_str(),*seria);
			for (StrList::iterator  it = seria->phyPortList.begin();it != seria->phyPortList.end();++ it)
			{
				pPortList.push_back(*it);
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理端口列表" + string("失败！") + string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPhyPortKeyByInSignalID(const ::std::string& signalKey, ::std::string& pPortKey)//根据信号获取端口;
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strInSignalLibName);
		if(curRDB->open())
		{
			CRdbInSignal *seria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			curRDB->get(signalKey.c_str(),*seria);
			pPortKey = seria->portIndex;
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理端口" + string("失败！") + string("信号key为：") + signalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetPhyPortKeyByOutSignalID(const ::std::string& signalKey, ::std::string& pPortKey)//根据信号获取端口;
{
	try
	{
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strOutSignalLibName);
		if(curRDB->open())
		{
			CRdbOutSignal *seria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
			curRDB->get(signalKey.c_str(),*seria);
			pPortKey = seria->portIndex;
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得装置的物理端口列表" + string("失败！") + string("信号key为：") + signalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

//模型编辑接口;
bool SCLModelServerImpl::AddVoltageLevel(const ::std::string& voltageLevelName)//增加电压等级;
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strVoltageLevelLibName);
		if(curRDB->open())
		{
			CRdbVoltageLevel *seria = (CRdbVoltageLevel *)rdbFac->createDBSerialize(VOLTAGELEVEL);
			seria->ID = voltageLevelName;
			seria->name = voltageLevelName;
			seria->desc = voltageLevelName;

			if(curRDB->put(voltageLevelName.c_str(),*seria))
			{
				rtn = true;
			}	
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;
		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"增加电压等级" + string("失败！")
			+ string("电压等级的key为：") + voltageLevelName;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::DeleteVoltageLevel(const ::std::string& voltageLevelName)//删除电压等级
{
	//SCDUnLoader scdUnLoader;
	return scdUnLoader.UnLoadByVolatageKey(voltageLevelName);
}

bool SCLModelServerImpl::AddSubstation(const ::std::string& voltageLevelKey, const ::std::string& subName/*, const std::vector<char>& fileData*/)//增加变电站模型
{
	try
	{
		qDebug()<<::GetCurrentProcessId()<<"CMyLoadingThread::run	SCLModelServerImpl::AddSubstation	1";
		QString path;
		path = QCoreApplication::applicationDirPath();
		path = path + "/../data/temp.scd";
		//QFile file(path);
		QFile file(subName.c_str());
		if (!file.open(QIODevice::ReadOnly)) 
		{
			qDebug()<<::GetCurrentProcessId()<<"CMyLoadingThread::run	SCLModelServerImpl::AddSubstation	1	false";
			return false;
		} 
		qDebug()<<::GetCurrentProcessId()<<"CMyLoadingThread::run	SCLModelServerImpl::AddSubstation	2";
		//else
		//{
		//	QByteArray tempArray;
		//	for(int i=0;i<fileData.size();i++)
		//	{
		//		tempArray.push_back(fileData.at(i));
		//	}

		//	QDataStream stream(&file);
		//	stream.device()->write(tempArray);
		//	file.close();
		//}

		//SCDLoader scdLoader;
		bool isSuc = scdLoader.LoadToMem(subName);//.toLocal8Bit().constData());
		
		if (!isSuc)
		{
			qDebug()<<::GetCurrentProcessId()<<"CMyLoadingThread::run	SCLModelServerImpl::AddSubstation	2	false";
			return false;
		}
		qDebug()<<::GetCurrentProcessId()<<::GetCurrentProcessId()<<"SCLModelServerImpl::AddSubstation	scdLoader.LoadToRDB	Start";
		isSuc = scdLoader.LoadToRDB(voltageLevelKey,"屏柜");
		qDebug()<<::GetCurrentProcessId()<<::GetCurrentProcessId()<<"SCLModelServerImpl::AddSubstation	scdLoader.LoadToRDB	End";
		if (!isSuc)
		{
			return false;
		}
		return true;
	}
	catch (DbRunRecoveryException &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbRunRecoveryException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	catch (DbMemoryException  &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbMemoryException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	catch (DbDeadlockException  &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbDeadlockException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	catch (DbRepHandleDeadException  &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbRepHandleDeadException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	catch (DbLockNotGrantedException  &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbLockNotGrantedException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	catch (DbException &ex)
	{
		qDebug()<<::GetCurrentProcessId()<<"************DbException**************"<<ex.get_errno()<<"---------"<<ex.what();
		return false;
	}
	//catch (__DB_STD(exception) &ex)
	//{
	//	qDebug()<<::GetCurrentProcessId()<<"************Exception**************"<<ex.what();
	//	return false;
	//}
	//catch (...)
	//{
	//	
	//	//QString str = eptn;
	//	string msg = (string)"模型服务:" + (string)"增加电压等级" + string("失败！")
	//		+ string("变电站名称为：") + subName;/* + string("error message:") + str.toLocal8Bit().constData();*/
	//	LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
	//	return false;
	//}
}

int SCLModelServerImpl::GetSubstationAddedProgress()//获得当前导入进度
{
	return scdLoader.GetCurProgress();
}

bool SCLModelServerImpl::DeleteSubstation(const ::std::string& substationKey)//删除变电站模型
{
	//SCDUnLoader scdUnLoader;
	return scdUnLoader.UnLoadBySubstationKey(substationKey);
	//return scdUnLoader.UnloadBySubstationKeyTest(substationKey);
}

bool SCLModelServerImpl::UpdateSubstationModel(const ::std::string& voltageLevelKey, const ::std::string& subName, const ::std::string& SCDFilePath)//更新变电站模型
{
	return false;
}

bool SCLModelServerImpl::AddPanelForSubstation(const ::std::string& subKey, const ::std::string& paneName)//为变电站增加屏柜
{
	return false;
}

bool SCLModelServerImpl::DeletePanelForSubstation(const ::std::string& PanelKey)//为变电站删除屏柜
{
	return false;
}

bool SCLModelServerImpl::AddIEDForPanel(const ::std::string& panelKey, const ::std::string& IEDName)//为屏柜增加装置
{

	return false;
}

bool SCLModelServerImpl::DeleteIEDForPanel(const ::std::string& IEDKey)//为屏柜删除装置
{
	return false;
}

bool SCLModelServerImpl::AddPortForIED(const ::std::string& IEDKey, const ::std::string& portName)//为装置增加物理端口
{
	return false;
}

bool SCLModelServerImpl::DeletePortForIED(const ::std::string& portKey)//为装置删除物理端口
{
	return false;
}

bool SCLModelServerImpl::AddInSignalForIED(const ::std::string& IEDKey, const ::std::string& inSignalName)//为装置增加输入信号
{
	return false;
}

bool SCLModelServerImpl::DeleteInSignalForIED(const ::std::string& inSignalKey)//为装置删除输入信号
{
	return false;
}

bool SCLModelServerImpl::AddOutSignalForIED(const ::std::string& IEDKey, const ::std::string& outSignalName) //为装置增加输出信号
{
	return false;
}

bool SCLModelServerImpl::DeleteOutSignalForIED(const ::std::string& outSignalKey)//为装置删除输出信号
{
	return false;
}

bool SCLModelServerImpl::AddFunction(const ::std::string& IEDKey, const ::std::string& funName, const ::std::string& funcDesc)//为装置增加功能
{
	try
	{
		bool rtn = true;
		string functionKey = IEDKey + "." + funName;
		string funcKeyOfIed = funName;
		CFunction func_rec;
		func_rec.name = funName;
		func_rec.desc = funcDesc;

		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		CRdb *funcRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);
		funcRDB->setDbName(strFunctionTripLibName);

		if(curRDB->open()&&funcRDB->open())
		{
			CRdbIed *seriaIed = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(IEDKey.c_str(),*seriaIed))
			{
				seriaIed->funcList.push_back(funcKeyOfIed);
				if(!curRDB->update(IEDKey.c_str(),*seriaIed))
				{
					rtn = false;
				}	
			}

			CRdbFunction *seriaFunc = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
			seriaFunc->set(func_rec);
			string funcKey = GetChildKeyFromParentKeyAndChildName(IEDKey,funName);
			if(!funcRDB->put(funcKey.c_str(),*seriaFunc))
			{
				rtn = false;
			}	

			delete seriaFunc;
			delete seriaIed;
			curRDB->close();
			funcRDB->close();
		}
		else
		{
			rtn = false;
		}
		delete curRDB;
		delete funcRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"为装置增加功能" + string("失败！")
			+ string("装置key为：") + IEDKey
			+ string("功能名为：") + funName;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::ModifyFunction(const ::std::string& funKey, const ::std::string& funcDesc)//为装置修改功能描述  
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strFunctionTripLibName);

		if(curRDB->open())
		{
			CRdbFunction *seria = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
			if(curRDB->get(funKey.c_str(),*seria))
			{
				seria->desc = funcDesc;
				if(curRDB->update(funKey.c_str(),*seria))
				{
					rtn = true;
				}	
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"修改功能描述" + string("失败！")
			+ string("功能key为：") + funKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::DeleteFunction(const ::std::string& funKey)//为装置删除功能 
{
	try
	{
		bool rtn = true;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		CRdb *iedRDB = rdbFac->createRDB(env); 
		curRDB->setDbName(strFunctionTripLibName);
		iedRDB->setDbName(strIedLibName);
		string iedKey = GetParentKey(funKey);

		if(curRDB->open()&&
			iedRDB->open())
		{
			if(!curRDB->del(funKey.c_str()))
			{
				rtn = false;
			}

			CRdbIed *seriaIed = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(iedRDB->get(iedKey.c_str(),*seriaIed))
			{
				string functionKey = GetNameFromKey(funKey);
				seriaIed->funcList.remove(functionKey);
				if(!iedRDB->update(iedKey.c_str(),*seriaIed))
				{
					rtn = false;
				}
			}
			delete seriaIed;
			iedRDB->close();
			curRDB->close();
		}
		else
		{
			rtn = false;
		}
		delete curRDB;
		delete iedRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"删除功能" + string("失败！")
			+ string("功能key为：") + funKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetDescForPort(const ::std::string& portKey, const ::std::string& desc)//设置物理端口的描述  
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);

		if(curRDB->open())
		{
			CRdbPhysicsPort *seria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
			if(curRDB->get(portKey.c_str(),*seria))
			{
				seria->desc = desc;
				if(curRDB->update(portKey.c_str(),*seria))
				{
					rtn = true;
				}	
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置物理端口描述" + string("失败！")
			+ string("端口key为：") + portKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetInSignalsListOfOutSignal(const ::std::string& outSignalKey, const std::vector<std::string>& inSignalKeys)//设置输出信号关联的输入信号列表（两个装置之间）
{
	return false;
}

bool SCLModelServerImpl::SetOutSignalsListOfInSignal(const ::std::string& inSignalKey, const std::vector<std::string>& outSignalKeys)//设置输入信号关联的输出信号列表（两个装置之间）
{
	return false;
}

bool SCLModelServerImpl::SetInternalOutSignalsListOfInSignal(const ::std::string& inSignalKey, const std::vector<std::string>& outSignalKeys)//设置输入信号关联的输出信号列表（装置内部）
{
	return false;
}

bool SCLModelServerImpl::SetInternalInSignalsListOfOutSignal(const ::std::string& outSignalKey, const std::vector<std::string>& inSignalKeys)//设置输出信号关联的输入信号列表（装置内部）
{
	return false;
}

std::string SCLModelServerImpl::GetIedKeyFromSignal(const std::string& keystring)
{
	string iedKey = keystring;
	for (int i = 0;i < 4;i ++)
	{
		size_t pos = iedKey.rfind('.');
		iedKey = iedKey.substr(0,pos);
	}
	return iedKey;
}

//????是否还需要再写入到Ied下属的关联中？
bool SCLModelServerImpl::AddFunctionOfInSignal(const ::std::string& inSignalKey, const ::std::string& funKey)//为输入信号增加功能关联
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);
		CRdb *iedRDB = rdbFac->createRDB(env);
		iedRDB->setDbName(strIedLibName);

		if(inSignalRDB->open()&&
			iedRDB->open())
		{
			CRdbInSignal *inSignalseria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			if(inSignalRDB->get(inSignalKey.c_str(),*inSignalseria))
			{
				bool found = false;
				for (StrList::iterator it = inSignalseria->funcList.begin();it != inSignalseria->funcList.end(); ++it)
				{
					if (funKey == *it)
						found = true;
				}

				if (!found)
				{
					inSignalseria->funcList.push_back(funKey);
					bool flag2 = inSignalRDB->update(inSignalKey.c_str(),*inSignalseria);
					//下边同步更新Ied下对应的功能列表
					//string iedKey = GetParentKey(inSignalKey);
					//CRdbIed *iedSeria = (CRdbIed *)rdbFac->createDBSerialize(IED);
					//if(iedRDB->get(iedKey.c_str(),*iedSeria))
					//{
					//	iedSeria->funcList.push_back(funKey);
					//	iedRDB->put(iedKey.c_str(),*iedSeria);
					//	rtn = true;
					//}
					//delete iedSeria;
				}
			}
			delete inSignalseria;
			inSignalRDB->close();
			iedRDB->close();
		}
		delete inSignalRDB;
		delete iedRDB;
		delete rdbFac;
		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"为输入信号增加功能关联" + string("失败！")
			+ string("输入信号key为：") + inSignalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::DeleteFunctionOfInSignal(const ::std::string& inSignalKey,const std::vector<std::string>& funcKeys)//为输入信号删除功能关联 
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);

		if(inSignalRDB->open())
		{
			CRdbInSignal *inSignalseria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			if(inSignalRDB->get(inSignalKey.c_str(),*inSignalseria))
			{
				for (uint i = 0;i <funcKeys.size(); ++ i)
				{
					bool found = false;
					for (StrList::iterator j = inSignalseria->funcList.begin();j != inSignalseria->funcList.end(); ++ j)
					{
						if(funcKeys[i] == *j)
						{
							found = true;
							break;
						}
					}
					if (found)
					{
						inSignalseria->funcList.remove(funcKeys[i]);
						if(inSignalRDB->update(inSignalKey.c_str(),*inSignalseria))
						{
							rtn = true;
						}
					}
				}
			}
			delete inSignalseria;
			inSignalRDB->close();
		}
		delete inSignalRDB;
		delete rdbFac;
		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"为输入信号删除功能关联" + string("失败！")
			+ string("输入信号key为：") + inSignalKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::AddTripForIED(const ::std::string& IEDKey, const ::std::string& tripName)//增加压板
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *iedRDB = rdbFac->createRDB(env);
		CRdb *tripRDB = rdbFac->createRDB(env);
		iedRDB->setDbName(strIedLibName);
		tripRDB->setDbName(strSoftTripLibName);

		if(iedRDB->open()&&
			tripRDB->open())
		{
			CRdbIed *seriaIed = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(iedRDB->get(IEDKey.c_str(),*seriaIed))
			{
				string tripKeyOfIED = tripName;
				bool found = false;
				for (StrList::iterator it = seriaIed->softTripList.begin();it != seriaIed->softTripList.end(); ++it)
				{
					if (tripKeyOfIED == *it)
					{
						found = true;
						break;
					}
				}	
				if (!found)
				{
					seriaIed->softTripList.push_back(tripKeyOfIED);
					if(iedRDB->update(IEDKey.c_str(),*seriaIed))
					{
						CSoftTrip trip_rec;
						trip_rec.name = tripName;
						CRdbSoftTrip *seriaTrip = (CRdbSoftTrip *)rdbFac->createDBSerialize(SOFTTRIP);
						seriaTrip->set(trip_rec);
						if(tripRDB->put(IEDKey.c_str(),*seriaTrip))
						{
							rtn = true;
						}
						delete seriaTrip;
					}
				}
			}	
			delete seriaIed;
			iedRDB->close();
			tripRDB->close();
		}
		delete iedRDB;
		delete tripRDB;
		delete rdbFac;
		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"为装置增加压板" + string("失败！")
			+ string("装置key为：") + IEDKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::DeleteTripForIED(const ::std::string& tripKey)//删除压板 
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *iedRDB = rdbFac->createRDB(env);
		CRdb *tripRDB = rdbFac->createRDB(env);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		CRdb *portRDB = rdbFac->createRDB(env);
		CRdb *funcRDB = rdbFac->createRDB(env);

		iedRDB->setDbName(strIedLibName);
		tripRDB->setDbName(strSoftTripLibName);
		inSignalRDB->setDbName(strInSignalLibName);
		outSignalRDB->setDbName(strOutSignalLibName);
		portRDB->setDbName(strPhysicsPortLibName);
		funcRDB->setDbName(strFunctionTripLibName);

		if(iedRDB->open()&&
			tripRDB->open()&&
			inSignalRDB->open()&&
			outSignalRDB->open()&&
			portRDB->open()&&
			funcRDB->open())
		{
			if(tripRDB->del(tripKey.c_str()))
			{
				string iedKey = GetParentKey(tripKey);
				CRdbIed *iedSeria = (CRdbIed *)rdbFac->createDBSerialize(IED);
				if(iedRDB->get(iedKey.c_str(),*iedSeria))
				{
					string tripKeyOfIed = GetNameFromKey(tripKey);
					iedSeria->softTripList.remove(tripKeyOfIed);
					if(iedRDB->update(iedKey.c_str(),*iedSeria))
					{
						for (StrList::iterator it = iedSeria->inSignalList.begin();it != iedSeria->inSignalList.end(); ++ it)
						{
							CRdbInSignal *inSignalSeria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
							if(inSignalRDB->get((*it).c_str(),*inSignalSeria))
							{
								string tripKeyOfIed = GetNameFromKey(tripKey);
								inSignalSeria->softTripKey.clear();
								if(inSignalRDB->update((*it).c_str(),*inSignalSeria))
								{

								}
							}
							delete inSignalSeria;
						}

						for (StrList::iterator it = iedSeria->outSignalList.begin();it != iedSeria->outSignalList.end(); ++ it)
						{
							CRdbOutSignal *outSignalSeria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
							if(outSignalRDB->get((*it).c_str(),*outSignalSeria))
							{
								outSignalSeria->softTripKey.clear();
								if(outSignalRDB->update((*it).c_str(),*outSignalSeria))
								{

								}
							}
							delete outSignalSeria;
						}

						for (StrList::iterator it = iedSeria->funcList.begin();it != iedSeria->funcList.end(); ++ it)
						{
							CRdbFunction *funcSeria = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
							if(funcRDB->get((*it).c_str(),*funcSeria))
							{
								funcSeria->m_SoftTrip = "";
								if(funcRDB->update((*it).c_str(),*funcSeria))
								{

								}
							}
							delete funcSeria;
						}

						for (StrList::iterator it = iedSeria->physicsPortList.begin();it != iedSeria->physicsPortList.end(); ++ it)
						{
							CRdbPhysicsPort *portSeria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
							if(portRDB->get((*it).c_str(),*portSeria))
							{
								portSeria->tripKey = "";
								if(portRDB->update((*it).c_str(),*portSeria))
								{

								}
							}
							delete portSeria;
						}
					}
				}
				delete iedSeria;
			}	
			iedRDB->close();
			tripRDB->close();
			inSignalRDB->close();
			outSignalRDB->close();
			portRDB->close();
			funcRDB->close();
		}
		delete iedRDB;
		delete tripRDB;
		delete inSignalRDB;
		delete outSignalRDB;
		delete portRDB;
		delete funcRDB;

		delete rdbFac;
		return true;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"删除压板" + string("失败！")
			+ string("压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetFunctionTripKey(const ::std::string& functionKey, const ::std::string& tripKey)//设置功能的功能压板（可能不存在） 
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strFunctionTripLibName);

		if(curRDB->open())
		{
			CRdbFunction *seria = (CRdbFunction *)rdbFac->createDBSerialize(FUNCTION);
			if(curRDB->get(functionKey.c_str(),*seria))
			{
				seria->m_SoftTrip = tripKey;
				if(curRDB->update(functionKey.c_str(),*seria))
				{
					rtn = true;
				}
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置功能压板" + string("失败！")
			+ string("功能压板key为：") + functionKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetIEDTripKey(const ::std::string& IEDKey, const ::std::string& tripKey)//设置设备的压板（表示断电一类的操作） 
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strIedLibName);

		if(curRDB->open())
		{
			CRdbIed *seria = (CRdbIed *)rdbFac->createDBSerialize(IED);
			if(curRDB->get(IEDKey.c_str(),*seria))
			{
				bool found = false;
				for (StrList::iterator it = seria->softTripList.begin();it != seria->softTripList.end(); ++it)
				{
					if (tripKey == *it)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					seria->softTripList.push_back(tripKey);
					//此处代码不正确，需要为先IED增加压板
				}
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置装置压板" + string("失败！")
			+ string("装置key为：") + IEDKey
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetPortTripKey(const ::std::string& portKey, const ::std::string& tripKey)//设置端口的压板（表示拨光纤一类的操作）
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strPhysicsPortLibName);

		if(curRDB->open())
		{
			CRdbPhysicsPort *seria = (CRdbPhysicsPort *)rdbFac->createDBSerialize(PHYSICSPORT);
			if(curRDB->get(portKey.c_str(),*seria))
			{
				seria->tripKey = tripKey;
				if(curRDB->update(portKey.c_str(),*seria))
				{
					rtn = true;
				}
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置端口压板" + string("失败！")
			+ string("端口key为：") + portKey
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::SetSignalTripKey(const ::std::string& signalKey, const ::std::string& tripKey)//设置信号的压板（软压板）  
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *inSignalRDB = rdbFac->createRDB(env);
		inSignalRDB->setDbName(strInSignalLibName);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		outSignalRDB->setDbName(strOutSignalLibName);

		if(inSignalRDB->open()&&
			outSignalRDB->open())
		{
			CRdbInSignal *inSignalSeria = (CRdbInSignal *)rdbFac->createDBSerialize(INSIGNAL);
			if(inSignalRDB->get(signalKey.c_str(),*inSignalSeria))
			{
				inSignalSeria->softTripKey = tripKey;
				if(inSignalRDB->update(signalKey.c_str(),*inSignalSeria))
				{
					rtn = true;
				}
			}
			delete inSignalSeria;

			if(!rtn)
			{
				CRdbOutSignal *outSignalSeria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);
				if(inSignalRDB->get(signalKey.c_str(),*outSignalSeria))
				{
					outSignalSeria->softTripKey = tripKey;
					if(inSignalRDB->update(signalKey.c_str(),*outSignalSeria))
					{
						rtn = true;
					}
				}
				delete outSignalSeria;
			}
			inSignalRDB->close();
			outSignalRDB->close();
		}
		delete inSignalRDB;
		delete outSignalRDB;
		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置虚信号压板" + string("失败！")
			+ string("虚信号key为：") + signalKey
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());
		return false;
	}
}

bool SCLModelServerImpl::GetSoftTripState(const ::std::string& tripKey, bool& state)//获得压板状态
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSoftTripLibName);

		if(curRDB->open())
		{
			CRdbSoftTrip *seria = (CRdbSoftTrip *)rdbFac->createDBSerialize(SOFTTRIP);
			if(curRDB->get(tripKey.c_str(),*seria))
			{
                state = seria->state;
				rtn = true;
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得压板状态" + string("失败！")
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());	
		return false;
	}
}

bool SCLModelServerImpl::SetSoftTripState(const ::std::string& tripKey, bool state)//设置压板状态
{
	try
	{
		bool rtn = false;
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *curRDB = rdbFac->createRDB(env);
		curRDB->setDbName(strSoftTripLibName);

		if(curRDB->open())
		{
			CRdbSoftTrip *seria = (CRdbSoftTrip *)rdbFac->createDBSerialize(SOFTTRIP);
			if(curRDB->get(tripKey.c_str(),*seria))
			{
				seria->state = state;
				if(curRDB->update(tripKey.c_str(),*seria))
				{
					rtn = true;
				}	
			}
			delete seria;
			curRDB->close();
		}
		delete curRDB;
		delete rdbFac;

		return rtn;
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"设置压板状态" + string("失败！")
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());	
		return false;
	}
}

bool SCLModelServerImpl::GetSignalKeyOfTrip(const ::std::string& tripKey,std::string &signalKey)
{
	try
	{
		string rtnStr = "";
		CFacFactory fac;
		CRDBFactory *rdbFac = fac.createFactory(BERKELEYDB_FAC);
		CRdb *outSignalRDB = rdbFac->createRDB(env);
		outSignalRDB->setDbName(strOutSignalLibName);
		outSignalRDB->open();

		CRdbOutSignal *seria = (CRdbOutSignal *)rdbFac->createDBSerialize(OUTSIGNAL);

		CRDBCursor *cursor = rdbFac->createRDBCursor();
		bool bFind = outSignalRDB->getFirst(*seria,cursor);
		if(bFind)
		{
			do
			{
				if(seria->softTripKey == tripKey)
				{
					rtnStr = seria->ID;
					break;
				}
			} 
			while(outSignalRDB->getNext(*seria,cursor));
		}

		outSignalRDB->close();
		delete seria;
		delete outSignalRDB;
		delete rdbFac;

		if(rtnStr=="")
		{
			return false;
		}
		else
		{
			signalKey = rtnStr;
			return true;
		}
	}
	catch (...)
	{
		string msg = (string)"模型服务:" + (string)"获得压板的虚信号" + string("失败！")
			+ string(" 压板key为：") + tripKey;
		LogRecorder::GetInstance()->WriteErrorMsg(msg.c_str());	
		return false;
	}
}
