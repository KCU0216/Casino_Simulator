# 다음 작업 순서

## 1. 셔플 제거 확인

게임 맵에서 블랙잭을 한 판 진행한다. 첫 베팅 후 카드 뭉치와 섞기 동작 없이 첫 카드 배분이 시작되어야 한다. 자리 0~3으로 배분할 때 딜러 몸이 돌아가고, 딜러 본인 카드를 놓을 때 기본 방향으로 돌아가는지 확인한다.

`/Game/Blackjack/BluePrint/BP_BlackjackTable`의 클래스 기본값 → `Blackjack | Presentation`에서 `Animate Dealer`는 켜고 `Animate Dealer Shuffle`은 끈다. 현재 코드와 BP 기본값은 이 상태로 설정한다. 맵에 놓은 인스턴스에 개별 값이 있으면 해당 인스턴스도 확인한다.

멀티에서는 서로 다른 자리 2개에 앉아 첫 배분·히트·스플릿·정산·다음 판을 확인한다. 한 장이 두 번 생성되거나 다른 자리로 가면, 자리 번호와 재현 상황을 기록한다.

## 2. 메인메뉴·로비 사용 확인

위젯은 `/Game/Blackjack/Level`에 있다. `WBP_MainMenu`, `WBP_LobbyUI`, `WBP_LobbyPlayerEntry`, `WBP_RoomEntry`를 확인한다. 플레이어 표시명·방 이름·준비 상태·음성 컨트롤의 기존 변수 이름을 유지한다.

실제 메인메뉴에서 로그인 → 방 만들기 → 두 번째 PC에서 검색·참가 → 준비 → 시작 → 로비 나가기를 확인한다. 상대 음소거와 볼륨은 내 화면에서 듣는 소리에 영향을 주어야 한다. 내 행의 마이크 음소거와 볼륨은 내 입력에 적용되어야 한다. 16:9, 21:9, 4:3에서 긴 이름이 버튼을 덮지 않는지 확인한다.

## 3. 방 만들기 입력창 디자인 준비

현재는 `WBP_MainMenu`의 `LobbyButton`을 누르면 바로 `CreateRoom("테스트 방")`을 호출한다. 제목 인자는 이미 C++에 있지만, 비밀번호 생성·검증·잠금 표시 API는 아직 없다. 아래는 우선 디자인만 준비하는 단계다.

1. `/Game/Blackjack/Level`에서 사용자 위젯 BP `WBP_CreateRoomPopup`을 만든다. 부모는 `UserWidget`으로 둔다.
2. 루트 `Canvas Panel` 아래 `Overlay`를 넣고, Canvas 슬롯 앵커를 전체 화면으로 설정한다. 좌우상하 오프셋은 0으로 둔다.
3. Overlay 첫 자식은 전체 화면을 덮는 반투명 검정 `Border`로 만든다.
4. 다음 자식은 중앙 정렬한 `SizeBox`로 만들고 Width Override 520을 설정한다. 안에 짙은 배경 `Border`, Padding 28, `Vertical Box`를 넣는다.
5. Vertical Box에 순서대로 다음 위젯을 넣는다.

| 종류 | 변수 이름 | 표시/설정 |
|---|---|---|
| TextBlock | TitleText | 방 만들기 |
| TextBlock | RoomNameLabel | 방 제목 |
| EditableTextBox | RoomNameInput | 힌트: 방 제목을 입력하세요 |
| TextBlock | PasswordLabel | 비밀번호 (선택) |
| EditableTextBox | PasswordInput | Is Password 켜기, 힌트: 비워두면 공개방 |
| TextBlock | ErrorText | 빈 문자열, 붉은색, 자동 줄바꿈 |
| HorizontalBox | ButtonRow | 취소/만들기 버튼을 가로 배치 |
| Button | CancelButton | 취소 |
| Button | CreateButton | 만들기 |

입력창·버튼·ErrorText의 `Is Variable`을 켠다. 입력창 높이는 44~48, 버튼 높이는 48 정도로 두고 행 사이 간격은 12~16으로 둔다. `ErrorText`는 빈 텍스트여도 자리를 크게 차지하지 않도록 한다. 제목은 서버에서 공백 정리 후 최대 48자로 제한하는 기존 규칙을 사용한다.

컴파일·저장하고 디자인 화면을 보내면 현재 함수에 맞춰 연결할 수 있다. 비밀번호는 아직 동작하지 않으므로, 입력창을 만든 것만으로 비공개방이 구현되었다고 판단하지 않는다.

## 4. 비밀번호 참가창 디자인 준비

`WBP_CreateRoomPopup`을 복제해 `WBP_JoinRoomPasswordPopup`으로 만든다. 방 제목 입력창 대신 선택한 방 제목을 보여줄 TextBlock을 두고, `PasswordInput`, `ErrorText`, `CancelButton`은 유지한다. `CreateButton`은 `JoinButton`으로 바꾸고 글자를 참가로 설정한다.

참가창은 입력이 틀렸을 때 오류를 보여주고 다시 입력할 수 있어야 한다. 비밀번호를 방 목록에 직접 표시하거나 세션 검색 데이터에 넣는 연결은 하지 않는다. 향후 C++에서 잠금 여부, 서버 검증, 실패 후 세션 정리·재시도, 음성 입장 시점을 함께 연결해야 한다.

기존 `WBP_RoomEntry`의 `RoomInfo`, `RoomNameText`, `PlayerCountText`, `JoinButton` 이름은 유지한다. 잠금 표시를 위해 작은 TextBlock `LockText`를 추가하고 일단 기본 텍스트를 자물쇠 표시 또는 `비공개`로 둔다. 공개/비공개 Visibility 분기는 C++ 잠금 값이 추가된 뒤 연결한다.

## 5. 그다음 우선순위

방 입력창이 준비되면 먼저 전체 게임 흐름을 두 PC에서 확인한다. 로비 → 1일차 → 모든 플레이어 납부(0원 포함) → 성공/실패 → 다음 날 → 재시작/메인메뉴까지 한 번에 이어서 진행한다. 그다음 블랙잭 UI 위치·카드 크기·자리별 카메라를 조정하고 마지막에 상점 NPC 외형을 선택한다.

소스가 추가되거나 함수 인자가 바뀐 뒤에는 에디터를 종료하고 전체 빌드 후 다시 연다. 디자이너 배치·텍스트·색만 바꿀 때는 BP 컴파일·저장으로 확인한다.
