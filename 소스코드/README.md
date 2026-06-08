# Blitz of Card

`Blitz of Card`는 Windows 콘솔 기반으로 동작하는 ASCII 카드 배틀 C++ 프로젝트입니다.

## 저장소 개요

- 솔루션 파일: `Blitz of Card.sln`
- 메인 소스 폴더: `Blitz of Card/`
- 배포용 실행 파일 폴더: `dist/x64-Release/`
- 기본 원격 저장소: `https://github.com/LocomotiveMaker/Slay-The-Spire.git`

## GitHub Release 패키지

- GitHub `Releases` 업로드용 압축 파일: `dist/Blitz-of-Card-win64.zip`
- 압축을 풀면 바로 실행 가능한 폴더 이름: `Blitz-of-Card-win64/`
- 포함 항목: `Blitz of Card.exe`, `Run-Fullscreen.bat`, `Run-Windowed.bat`, `README.txt`, `Assets/`
- 재생성 스크립트: `tools/Package-GitHubRelease.ps1`

## 실행 환경

- 64비트 Windows 환경이 필요합니다.
- 반드시 `Windows Console Host`로 실행해야 합니다.
- `Windows Terminal`에서는 기존 콘솔 렌더링 방식 때문에 검은 화면이 나오거나 정상 출력이 깨질 수 있습니다.
- 실행 파일 옆의 `Assets` 폴더를 제거하면 안 됩니다.

## 빌드 방법

- `Visual Studio 2022`와 저장소 루트의 `.vsconfig` 기준 구성 요소가 필요합니다.
- 필수 구성 요소는 `Desktop development with C++`와 `Windows 10 SDK 10.0.19041.0` 이상입니다.
- `windows.h`, `winnt.h`, `winuser.h` 같은 외부 종속성 오류가 나오면 Visual Studio Installer에서 `.vsconfig`를 가져오면 됩니다.
- `Blitz of Card.sln`을 열고 `Debug | x64` 또는 `Release | x64`로 빌드합니다.
- 빌드 후 생성된 `Blitz of Card.exe`를 실행합니다.

명령줄 빌드:

```powershell
powershell -ExecutionPolicy Bypass -File tools/Build-Solution.ps1 -Configuration Release -Platform x64
```

## 실행 방법

- 권장 실행 파일: `dist/x64-Release/Blitz of Card.exe`
- 전체 화면 실행이 필요하면 `dist/x64-Release/Run-Fullscreen.bat`를 사용합니다.
- 전체 화면 문제가 있으면 `dist/x64-Release/Run-Windowed.bat`를 사용합니다.
- 어떤 방식이든 기본 콘솔 앱은 `Windows Console Host`로 설정하는 것이 전제입니다.

## 주요 수정 지점

- 전투 / 카드팩 / 사망 / Neow ASCII 아트:
  [AsciiArtLibrary.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/AsciiArtLibrary.cpp)
- 아트 ID 및 진입 지점:
  [AsciiArtLibrary.h](/C:/Slay-The-Spire/Blitz%20of%20Card/AsciiArtLibrary.h)
- 적 아트 라우팅:
  [EntityUI.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/EntityUI.cpp)
- 카드팩 씬 레이아웃 / 클리핑 / Neow 배치:
  [main.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/main.cpp)

## 참고

- 대부분의 아트 교체는 `AsciiArtLibrary.cpp`의 문자열 배열만 수정하면 됩니다.
- `Normalize(...)`가 공통 들여쓰기를 제거하므로 앞쪽 공백이 있는 ASCII 아트도 그대로 붙여 넣을 수 있습니다.
- 현재 적 아트 분기 규칙은 다음과 같습니다.
- `id >= 9200`: 보스 아트
- `id >= 9100`: 엘리트 아트
- 그 외: 일반 적 아트
- 화면 배치가 어긋나면 아트 문자열보다 `main.cpp`의 렌더 위치와 클리핑을 먼저 조정하는 편이 안전합니다.
