# 메인 메뉴·로비 UI 편집 가이드

현재 메뉴 위젯 네 개는 기존 BP 기능을 유지한 채 어두운 배경과 골드 버튼으로 정리했다. 별도 배경 이미지 없이 UMG 스타일과 패널로 구성한다. 상점 캐릭터 교체, 게임 HUD 디자인, 좌석 카메라·캐릭터 방향은 이 작업에 포함하지 않았다.

## 편집할 에셋

콘텐츠 브라우저의 `Content/Blackjack/Level`에서 연다.

| 위젯 | 역할 | 유지할 주요 이름 |
|---|---|---|
| `WBP_MainMenu` | 로그인, 방 만들기·검색, 설정, 종료 | `MainPanel`, `WidgetSwitcher_1`, `LoginPanel`, `MainMenuPanel`, `RoomListBox`, `StatusText` |
| `WBP_LobbyUI` | 플레이어 목록, 준비·시작, 나가기, 전체 음성 설정 | `PlayerListBox`, `ReadyButton`, `Ready_Text`, `LeaveButton`, `VerticalBox_58` |
| `WBP_LobbyPlayerEntry` | 플레이어 한 명의 이름·음성·준비 상태 | `PlayerNameText`, `SizeBox_0`, `PlayerVolumeSlider`, `PlayerMuteCheck`, `ReadyStateText` |
| `WBP_RoomEntry` | 검색된 방 한 개의 이름·인원·참가 버튼 | `SizeBox_17`, `HorizontalBox_29`, `RoomNameText`, `PlayerCountText`, `JoinButton` |

기존 위젯의 이름과 타입을 바꾸거나 삭제하면 BP의 변수 참조와 이벤트 연결이 깨질 수 있다. 장식 위젯은 `Casino...` 이름으로 추가했다. `WBP_MainMenu`의 부모 `CasinoMainMenuWidget`과 나머지 세 위젯의 `UserWidget` 부모는 유지한다. 내부 `WBP_Settings`도 기존 위젯을 사용한다.

## 현재 계층 구조

```text
WBP_MainMenu
└─ MainPanel
   ├─ CasinoThemeBackground / CasinoThemeHeaderRule
   ├─ StatusText: 화면 전환 후에도 보이는 온라인 상태·오류
   └─ WidgetSwitcher_1
      ├─ LoginPanel: 제목 + VerticalBox_2의 로그인·나가기
      ├─ MainMenuPanel
      │  ├─ VerticalBox_1: 방 만들기·방 찾기·설정·나가기
      │  └─ CasinoRoomFrame → CasinoRoomScroll → RoomListBox
      └─ WBP_Settings

WBP_LobbyUI
└─ CanvasPanel_32
   ├─ 제목·안내 문구·배경
   ├─ CasinoPlayerFrame → CasinoPlayerScroll → PlayerListBox
   ├─ CasinoVoiceFrame → VerticalBox_58
   ├─ ReadyButton
   └─ LeaveButton

WBP_LobbyPlayerEntry
└─ CasinoEntryHeight → CasinoPlayerEntryFrame → HorizontalBox_22
   ├─ PlayerNameText
   ├─ SizeBox_0 → PlayerVolumeSlider
   ├─ PlayerMuteCheck → CasinoPlayerMuteLabel
   └─ ReadyStateText

WBP_RoomEntry
└─ CasinoEntryHeight → CasinoRoomEntryFrame → SizeBox_17 → HorizontalBox_29
   ├─ RoomNameText
   ├─ PlayerCountText
   └─ JoinButton
```

리스트를 채우는 기존 함수는 `RoomListBox`와 `PlayerListBox`에 행을 추가한다. 새 ScrollBox는 많은 행을 화면 밖으로 넘기지 않고 스크롤하도록 감싼 것이다. 리스트 변수의 타입은 계속 `VerticalBox`다.

## 버튼과 음성 기능

메인 메뉴의 `LoginButton`, `LobbyButton`, `FindRoomsButton`, `SettingsButton`, 종료 버튼은 기존 이벤트를 그대로 사용한다. 방 참가 역시 `JoinButton`이 가진 기존 `JoinRoom(SearchIndex)` 연결을 사용한다. 참가 버튼의 기본 문구는 `참가`로 바꿨다.

기존 `StatusText`는 로그인 버튼 내부에서 `MainPanel` 하단으로 이동했다. 기존 `OnChanged`·`OnError` 연결이 같은 변수에 상태·오류를 쓰므로 로그인 페이지가 사라져도 메시지가 보인다. 로그인 버튼의 글자는 새 장식 `CasinoLoginButtonCaption`이 표시한다. 나중에 방 비밀번호 팝업을 만들면 팝업 내부에 별도 `ErrorText`를 두고, 공유 `StatusText`를 그 팝업 안으로 옮기지 않는다.

로비의 `ReadyButton`은 방장과 참가자의 동작을 기존 `RefreshLobby`와 클릭 이벤트에서 구분한다. 방장은 시작하고 참가자는 준비를 전환한다. 서버의 `CanStartHostedGame` 검사는 다른 플레이어들이 모두 준비했는지 확인한다. 버튼을 꾸미기 위해 이 분기나 서버 검사를 제거하지 않는다.

| 음성 UI | 현재 동작 |
|---|---|
| 내 행의 `PlayerVolumeSlider` | `SetVoiceInputVolume`: 내 마이크 입력 크기 |
| 내 행의 `PlayerMuteCheck` | `SetMicrophoneMuted`: 내 마이크 음소거 |
| 다른 사람 행의 `PlayerVolumeSlider` | `SetVoicePlayerVolume`: 내가 듣는 그 사람의 크기 |
| 다른 사람 행의 `PlayerMuteCheck` | `SetVoicePlayerMuted`: 내가 그 사람을 음소거 |
| `VoiceEnabledCheck` | 음성 채팅 사용 여부 |
| `PushToTalkCheck` | 눌러서 말하기 사용 여부 |
| `VoiceOutputSlider` | 내가 듣는 전체 음성 크기 |

내 행과 다른 사람 행은 `WBP_LobbyPlayerEntry`의 `bIsSelf` 분기로 구분한다. 새로운 행 위젯을 만들거나 슬라이더를 복제할 때 이 초기화·분기를 유지한다. 현재 눌러서 말하기 문구는 V 키를 표시한다. 설정에서 키를 바꾼다면 문구도 실제 키에 맞춰야 한다.

## 크기·위치·색상 수정

1. 먼저 디자이너에서 전체 페이지를 16:9, 4:3, 넓은 화면 비율로 확인한다.
2. 메인 메뉴의 좌측 버튼은 `VerticalBox_1`, 검색 목록은 `CasinoRoomFrame`의 Canvas 슬롯을 편집한다. 로비는 `CasinoPlayerFrame`, `CasinoVoiceFrame`, `ReadyButton`, `LeaveButton`의 슬롯을 편집한다.
3. 전체 영역은 늘어나는 앵커로 배치했다. 앵커를 한 점으로 바꾸면서 큰 고정 X 위치를 넣으면 작은 화면에서 잘릴 수 있다. 행 내부는 HorizontalBox의 Fill/Auto와 패딩으로 조절한다.
4. 플레이어 이름과 방 이름은 `Overflow Policy = Ellipsis`, `Clipping = Clip to Bounds`를 유지한다. 긴 이름이 인원 수·음소거·참가 버튼을 덮는 것을 막는다.
5. 카드 배경은 Border의 Brush, 버튼은 Style의 Normal/Hovered/Pressed/Disabled를 수정한다. 버튼 글자는 Foreground 색을 상속하므로 버튼 Style의 Foreground도 함께 확인한다.
6. 새로운 배경 이미지를 넣을 때는 입력을 막지 않도록 `Not Hit-Testable`로 두고 버튼보다 낮은 Z 순서를 지정한다.

편집 후 컴파일·저장한다. 수동으로 꾸민 뒤 `UIPolish` 마이그레이션 명령을 다시 실행하면 자동 스타일이 다시 적용되므로, 수동 디자인을 유지하려면 마이그레이션 실행 여부를 구분한다.

## 미리보기와 실제 테스트

실행 중인 에디터 콘솔에 `CasinoMenu.RenderPreviews`를 입력하면 `Saved/CasinoDealerSetup/MenuPreviews`에 12개의 PNG가 생성된다. 로그인, 빈 메인 메뉴, 방 목록이 있는 메인 메뉴, 로비를 각각 1920×1080, 2560×1080, 1440×1080으로 렌더링한다. 프로젝트 DPI와 논리 크기는 `preview-info.txt`에 기록한다.

이 도구는 실제 WidgetBlueprint를 디자인 모드의 임시 인스턴스로 렌더링한다. 샘플 이름·인원·준비 상태를 보여주며 로그인, 세션 참가, 마이크 동작을 실행하지 않는다. 따라서 PNG가 잘 보이는 것과 멀티플레이 기능이 정상인 것은 별도로 확인해야 한다.

실제 테스트에서는 다음을 확인한다.

- 로그인 후 메인 메뉴 이동, 방 생성·검색·참가, 설정 열기·닫기, 종료.
- 방장·클라이언트 모두에서 플레이어 목록과 준비 상태 갱신, 모두 준비하기 전 시작 차단, 로비 나가기.
- 내 마이크 볼륨·음소거와 다른 사람의 듣기 볼륨·음소거가 각각 올바른 대상을 바꾸는지.
- 음성 채팅 끄기·켜기, 눌러서 말하기, 전체 출력 볼륨.
- 긴 방 이름·플레이어 이름, 여러 검색 결과의 스크롤, 세 화면 비율에서 겹침 여부.
- 게임 맵으로 이동한 뒤 메뉴·로비 UI가 사라지고 기존 게임 UI 흐름으로 이어지는지.

다음 수동 디자인 작업은 실제 게임에서 읽기 쉬운 글자 크기와 여백 확인, 메뉴 배경 이미지 선택, 설정 화면까지 색상 통일이다. 기존 게임·상점 UI와 서버 기능을 이 메뉴 디자인 작업과 함께 다시 만들 필요는 없다.
