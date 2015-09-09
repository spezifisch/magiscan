#ifndef _BMP_H_
#define _BMP_H_

void SaveBitmapToFile( BYTE* pBitmapBits, LONG lWidth, LONG lHeight,WORD wBitsPerPixel, LPCTSTR lpszFileName );
void SaveBitmapToFileColor( BYTE* pBitmapBits, LONG lWidth, LONG lHeight,WORD wBitsPerPixel, LPCTSTR lpszFileName );

#endif // _BMP_H_
