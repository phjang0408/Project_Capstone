# 배포본 실행과 소스 확인

## Windows 배포본

1. [공유 폴더](https://drive.google.com/drive/folders/1iyVBmlqnq_v9_JxdqsMZOD4JRYPQMbg_?usp=drive_link)에서 `HelpWatson.zip`을 내려받습니다.
2. 전체 폴더 구조를 유지해 압축을 풉니다. 압축 크기는 약 1.37 GB이며 내부 파일의 합계는 약 1.93 GB입니다.
3. `HelpWatson/Windows/Capstone.exe`를 실행합니다.
4. 온라인 방 생성·검색·참가는 Steam에 로그인한 상태에서 확인합니다.
5. 방을 만든 뒤 설정한 인원이 모이면 게임을 진행합니다.

위 순서는 보고서와 배포본 파일 목록을 기준으로 정리했습니다. 문서 보완 과정에서는 실행 파일을 실행하지 않았습니다. 백엔드와 AI 서비스가 현재 운영 중인지, 배포본이 어떤 서버 주소로 연결되는지는 확인되지 않았습니다.

## 조작

| 입력 | 기능 |
| --- | --- |
| WASD / 마우스 | 이동 / 시점 조작 |
| Shift / Space | 달리기 / 점프 |
| E | 증거물 조사·수집 |
| I | 인벤토리와 증거 확인 |
| T | AI 조수 대화 |
| B | 최종 답안 작성 |
| Tab | 메뉴 |

## 소스에서 확인할 사항

이 저장소는 Unreal C++ 소스와 `.uproject`, 프로젝트 `Config`를 공개한 형태입니다. 맵·위젯·게임플레이 Blueprint가 들어 있는 `Content`와 프로젝트 플러그인(`AdvancedSessions`)은 포함되어 있지 않아 단독 재빌드가 가능한 구성이 아닙니다. Windows 배포본에도 이 프로젝트의 편집용 소스·에셋은 포함되어 있지 않습니다.

원본 Unreal 프로젝트를 복원한 뒤에는 엔진 버전(`Capstone.uproject` 기준 UE 5.6)과 `OnlineSubsystemSteam`, `SteamSockets`, `Http`, `Json`, `JsonUtilities` 의존성을 맞춰야 합니다. 공개 C++의 백엔드 주소는 `http://127.0.0.1:7777`로 고정되어 있으며, 서버 비용 방지를 위해 해당 HTTP 요청부는 현재 주석 처리되어 있습니다. 증거·문·최종 제출 경로와 방 관리 경로의 `/api` 접두어가 일관되지 않으므로 실제 서버 API와 대조가 필요합니다.
