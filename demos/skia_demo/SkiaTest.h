#ifndef __SKIATEST_H__
#define __SKIATEST_H__

class SkCanvas;


class CSkiaTest
{
public:
	~CSkiaTest(void);
private:
	CSkiaTest(void);
	CSkiaTest(const CSkiaTest& rhs);
	CSkiaTest& operator= (const CSkiaTest& rhs);

public:
	static CSkiaTest* getInstance();

	void Draw(SkCanvas * canvas);

	void next();
	void prev();

private:
	int getCountOfFuncs();
	int m_iCurrentFuncIndex;
};

#endif//__SKIATEST_H__
