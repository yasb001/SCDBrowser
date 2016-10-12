#include "SCDDifferCompare.h"

class CWordCreat
{
public:
	CWordCreat();
	~CWordCreat();

	void CreatHtml();


private:

	SCDDiffCompare *m_SCDDiffCompare;
	SCLNameTranslator m_NameTranslator;
	QString m_FileStream;
	QString m_SummaryInfo;
	QString m_TableInfo;

	int m_IEDCount;
	int m_SVCBCount;
	int m_GOOSECBCount;
	int m_InputCount;
	int m_PhyPortCount;
	int m_InSigCount;
	int m_OutSigCount;

	int m_Ord;

private:

	void AddAttachInfo();

	void AddSummaryInfo();

	void AddIEDDiff();
	void AddSVCBDiff();
	void AddGOOSECBDiff();
	void AddOutSignalDiff();
	void AddInSignalDiff();
	void AddInputsDiff();
	void AddPhyPortDiff();

	void CreatH1Title(QString H1Title);
	void CreatH2Title(QString H2Title);
	void CreatH2TitleWithoutOrd(QString H2Title);
	void CreatTable();

	bool IsIEDAttrDiff(IEDStru *pIED);
	bool IsSVCBAttrDiff(SVCBStru *pSV);
	bool IsGOOSECBAttrDiff(GOOSECBStru *pGOOSE);
	bool IsSignalAttrDiff(SignalStru *pSig);
	bool IsInputAttrDiff(ExtRefStru *pInput);
	bool IsPhyPortAttrDiff(PhysicsPortStru *pPhy);

	void StartTable();
	void EndTable();
	void AddVerifyInfo();

	void AddTrInfo(BasicStru basicStr);
	void AddTrInfo(BasicIntStru basicStr);

	void AddEndInfo();
	void SaveFile();

};

#pragma  comment(lib,"libword.lib")
