#pragma once
#define BUFSIZE_Serial 1400
#include <memory.h>
class CPacket
{
private:
	char* m_cpBuffer;
	int m_iFront, m_iRear;
	int m_iSize;
public:
	CPacket();
	CPacket(int iBufferSize);
	virtual	~CPacket();
	void moveRear(int a_move);
	void moveFront(int a_move);
	void clear();
	int	bufferSize();
	int useSize();
	char* getBufferPtr();
	int	getData(char* chpDest, int iSize);
	int putData(char* chpSrc, int iSrcSize);
	void resize();

	CPacket& operator << (int a_i);
	CPacket& operator >> (int& a_i);

	CPacket& operator << (unsigned int a_i);
	CPacket& operator >> (unsigned int& a_i);

	CPacket& operator << (unsigned char a_i);
	CPacket& operator >> (unsigned char& a_i);

	CPacket& operator << (unsigned short a_i);
	CPacket& operator >> (unsigned short& a_i);
};

CPacket::CPacket()
{
	m_cpBuffer = new char[BUFSIZE_Serial];
	m_iSize = 1400;
	m_iFront = 0;
	m_iRear = 0;
}
CPacket::CPacket(int iBufferSize)
{
	m_cpBuffer = new char[iBufferSize];
	m_iSize = 1400;
	m_iFront = 0;
	m_iRear = 0;
}
CPacket::~CPacket()
{

}
void CPacket::moveRear(int a_move)
{
	m_iRear = m_iRear + a_move;
}
void CPacket::moveFront(int a_move)
{
	m_iFront = m_iFront + a_move;
}
void CPacket::clear()
{
	m_iFront = 0;
	m_iRear = 0;
}
int	CPacket::bufferSize()
{
	return m_iSize;
}
int CPacket::useSize()
{
	return m_iRear - m_iFront;
}
char* CPacket::getBufferPtr()
{
	return m_cpBuffer;
}
int	CPacket::getData(char* chpDest, int iSize)
{
	if (iSize > useSize())
		iSize = useSize();
	memcpy(chpDest, m_cpBuffer, iSize);
	return iSize;
}
int CPacket::putData(char* chpSrc, int iSrcSize)
{
	if (iSrcSize > (m_iSize - useSize() - 1))
	{
		resize();
		putData(chpSrc, iSrcSize);
		return iSrcSize;
	}
	memcpy(&m_cpBuffer[m_iRear], chpSrc, iSrcSize);
	return iSrcSize;
}
void CPacket::resize()
{
	m_iSize = m_iSize + 100;
	char* temp = new char[m_iSize];
	memcpy(temp, m_cpBuffer, useSize());
	m_cpBuffer = temp;
}
CPacket& CPacket::operator << (int a_i)
{
	if (sizeof(a_i) > (m_iSize - useSize() - 1))
	{
		resize();
		operator << (a_i);
		return *this;
	}
	memcpy(&(m_cpBuffer[m_iRear]), &a_i, sizeof(a_i));
	m_iRear = m_iRear + sizeof(a_i);
	return *this;
}
CPacket& CPacket::operator >> (int& a_i)
{

	if (sizeof(a_i) > useSize())
	{
		throw sizeof(a_i);
	}
	memcpy(&a_i, &(m_cpBuffer[m_iFront]), sizeof(a_i));
	m_iFront = m_iFront + sizeof(a_i);

	return *this;
}

CPacket& CPacket::operator << (unsigned int a_i)
{
	if (sizeof(a_i) > (m_iSize - useSize() - 1))
	{
		resize();
		operator << (a_i);
		return *this;
	}
	memcpy(&(m_cpBuffer[m_iRear]), &a_i, sizeof(a_i));
	m_iRear = m_iRear + sizeof(a_i);
	return *this;
}
CPacket& CPacket::operator >> (unsigned int& a_i)
{

	if (sizeof(a_i) > useSize())
	{
		throw sizeof(a_i);
	}
	memcpy(&a_i, &(m_cpBuffer[m_iFront]), sizeof(a_i));
	m_iFront = m_iFront + sizeof(a_i);

	return *this;
}

CPacket& CPacket::operator << (unsigned char a_i)
{
	if (sizeof(a_i) > (m_iSize - useSize() - 1))
	{
		resize();
		operator << (a_i);
		return *this;
	}
	memcpy(&(m_cpBuffer[m_iRear]), &a_i, sizeof(a_i));
	m_iRear = m_iRear + sizeof(a_i);
	return *this;
}
CPacket& CPacket::operator >> (unsigned char& a_i)
{

	if (sizeof(a_i) > useSize())
	{
		throw sizeof(a_i);
	}
	memcpy(&a_i, &(m_cpBuffer[m_iFront]), sizeof(a_i));
	m_iFront = m_iFront + sizeof(a_i);

	return *this;
}

CPacket& CPacket::operator << (unsigned short a_i)
{
	if (sizeof(a_i) > (m_iSize - useSize() - 1))
	{
		resize();
		operator << (a_i);
		return *this;
	}
	memcpy(&(m_cpBuffer[m_iRear]), &a_i, sizeof(a_i));
	m_iRear = m_iRear + sizeof(a_i);
	return *this;
}
CPacket& CPacket::operator >> (unsigned short& a_i)
{

	if (sizeof(a_i) > useSize())
	{
		throw sizeof(a_i);
	}
	memcpy(&a_i, &(m_cpBuffer[m_iFront]), sizeof(a_i));
	m_iFront = m_iFront + sizeof(a_i);

	return *this;
}

