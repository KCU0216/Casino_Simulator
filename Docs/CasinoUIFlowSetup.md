# 게임 루프 UI 연결 순서

이번 C++ 변경은 화면을 관리하는 틀과 서버의 날짜 안내 단계를 추가한다. 아래 BP 연결은 에디터에서 해야 한다. 기존 `.uasset`은 수정하지 않았다.

## 1. 구조

```text
로컬 BP_FirstPersonPlayerController
 └─ CasinoUIRoot (C++에서 자동 생성)
     ├─ HUDLayer          기존 WBP_HUD: 잔액·상태·슬롯
     ├─ InteractionLayer  블랙잭·포커·사다리·주사위·상점 등
     └─ ModalLayer        메뉴·로비·날짜 안내·납부·결과

서버 GameMode → GameState의 LoopStatus 복제 → 각자의 PlayerController가 화면 전환
```

`WBP_UIRoot`를 새로 만들 필요는 없다. 세 개의 전체 화면 Overlay는 C++에서 만든다. 관리되는 위젯에서는 `Remove All Widgets`를 쓰지 않는다.

| UIScreen | 의미 | 기본 HUD |
|---|---|---|
| MainMenu | 메인메뉴 | 숨김 |
| Lobby | 대기실 | 숨김 |
| Loading | 게임 맵/참가자 준비 대기 | 숨김 |
| DayIntro | 며칠 차·목표 금액 안내 | 숨김 |
| Playing | 하루 진행 | 표시 |
| Payment | 납부 입력/다른 사람 대기 | 표시, BP에서 잔액만 |
| DayPassed | 오늘 납부 성공, 다음 날 대기 | 표시, BP에서 잔액만 |
| Result | 최종 클리어 또는 실패 | 숨김 |

메뉴·로비 구분은 프로젝트 설정 → Game → Casino Online의 Menu Map / Lobby Map을 사용한다. 해당 맵도 `BP_FirstPersonPlayerController`를 사용해야 한다. 기존 GameMode 설정을 확인한다.

## 2. 게임·상점 UI를 관리 대상으로 연결

각 UI를 만드는 기존 BP에서 `Create Widget`과 초기화 함수는 유지한다. `Owning Player`는 그 화면을 보는 로컬 플레이어 컨트롤러로 지정한다.

```text
기존: Create Widget → 변수 저장 → Add to Viewport → 기존 Init
변경: Create Widget → 변수 저장 → Show Interaction UI → Branch(Return Value)
                                                     True → 기존 Init
                                                     False → 변수 비우기
```

`Show Interaction UI`의 Target은 해당 `BP_FirstPersonPlayerController`, Widget은 만든 UI다. 게임 진행 상태가 아니면 False를 반환한다. False인데 이후에 직접 `Add to Viewport`하지 않는다.

적용 대상은 블랙잭뿐 아니라 포커·사다리·주사위·상점·교환 등이다. 하위 키패드를 게임 UI 안에 넣었다면 부모 UI만 등록하면 된다. 키패드를 따로 `Add to Viewport`했다면 그 위젯도 등록해야 한다. 인벤토리와 일시정지 화면의 C++ 생성 경로는 이미 연결했다.

평소 닫기에서는 `Remove from Parent` 대신 `Close Interaction UI(Widget)`를 사용한다. 이 함수는 화면을 제거한다. NPC/머신 사용 종료와 이동 복구를 위한 기존 `Close Current Interaction` 등의 서버 요청은 유지한다. UI만 지우는 것으로 머신 점유가 풀리지는 않는다.

컨트롤러의 `Event On Managed Widget Closed(Widget)`는 강제 종료에도 호출된다. 여기서 해당 위젯의 외부 이벤트 바인딩을 해제하고, 캐릭터/컨트롤러가 보관한 위젯 변수를 비운다. 위젯 자체와 그 WidgetTree에서 찾은 UserWidget의 애니메이션·Delay·타이머는 C++이 정리한다. Actor가 가진 타이머/델리게이트까지 자동 해제하지는 않는다.

게임 보상·베팅 종료는 기존 `CasinoDayParticipant`가 담당한다. 화면 닫기와 별개다. 슬롯머신 내부의 하루 종료 연결은 담당 팀원 작업으로 남겨뒀다.

## 3. 자동으로 보여줄 화면 등록

`BP_FirstPersonPlayerController` → 클래스 디폴트 → Casino → UI → `Screen Widget Classes`.

처음에는 다음 항목부터 등록한다.

| Key | Value |
|---|---|
| DayIntro | 새 `WBP_DayIntro` |
| Payment | 기존 `WBP_DailyPayment` |
| DayPassed | 새 `WBP_DayPassed` — 선택 사항 |
| Result | 새 `WBP_RunResult` |

메뉴/로비는 기존 화면 생성 경로를 정리한 뒤 MainMenu / Lobby에 등록한다. 등록한 화면은 C++이 한 번 만들고 상태가 끝나면 닫는다. 따라서 같은 화면을 레벨 BP/컨트롤러에서 따로 생성하는 경로는 제거한다. 빈 항목은 화면을 자동 생성하지 않지만 상태와 입력 전환은 적용된다.

자동 생성된 현재 화면은 컨트롤러의 `Active Screen Widget`이다. 각 화면의 `Event Construct`에서 `Get Owning Player → Cast to BP_FirstPersonPlayerController`로 필요한 참조를 가져온다.

기존 수동 생성 방식을 당분간 유지한다면 `Add to Viewport` 대신 `Show Screen UI`를 사용한다. Target=로컬 컨트롤러, Widget=생성한 화면, Expected Screen=Payment/MainMenu/Lobby 등 정확한 상태. 이 방식과 같은 상태의 `Screen Widget Classes` 자동 생성은 동시에 사용하지 않는다.

## 4. 기존 납부 이벤트 정리 — 자동 생성 방식을 쓸 때

`BP_FirstPersonPlayerController`에서:

- `On Prepare Daily Payment`의 기존 `Create WBP_DailyPayment → Add to Viewport`는 제거한다.
- `On Prepare Casino Day`, `On Finish Daily Payment`에서 화면을 따로 생성하는 연결도 제거한다. UI 닫기는 상태 전환이 처리한다.
- 기존 `On Close Gameplay UI For Payment`에서 `Remove All Widgets`를 호출하지 않는다. 외부 참조/바인딩 해제 작업이 있다면 유지할 수 있다.
- 입력 모드와 마우스 표시를 이 이벤트들에서 따로 되돌리지 않는다. 날짜 안내/납부/결과는 C++이 UI Only로 설정한다.

납부 결과 연결:

```text
Event On Daily Payment Result(bSuccess)
 → Get Active Screen Widget
 → Cast to WBP_DailyPayment
 → Show Payment Result(bSuccess)
```

Cast Failed는 아무것도 하지 않는다. 이미 납부가 종료되어 결과 화면으로 넘어갔을 수 있다. 이 이벤트에서 UI를 새로 만들지 않는다.

납부 위젯 내부의 숫자 입력·0원 제출·`Server Submit Daily Payment` 호출은 유지한다. 서버에 요청만 보낸 시점에 창을 닫지 않는다. 개별 납부 성공 시 입력 패널을 숨기고 대기 문구를 보여준다. 공동 목표 성공/실패와 개인 납부 요청 성공/실패는 서로 다르다.

## 5. HUD 잔액만 표시하는 기존 함수 연결

컨트롤러에 `Event On UI Loop Status Updated(Status)`를 추가한다.

```text
Status → Break Casino Loop Status → Phase
  Settling 또는 DayPassed이면 True, 그 외 False
 → Player HUD Widget → Cast to WBP_HUD → SetPaymentMode(True/False)
```

이 이벤트는 금액/상태 갱신마다 호출되므로 여기서 위젯을 매번 생성하지 않는다. `On UI Screen Changed`는 화면 상태가 바뀔 때만 호출된다. 복제 데이터가 늦게 도착하는 경우도 있으므로 날짜/결과 텍스트 갱신은 `On UI Loop Status Updated`에서도 실행한다.

같은 맵에서 날짜/납부 상태를 바꿀 때 HUD는 기존 인스턴스를 유지하며 부모 레이어만 숨긴다. 로비→게임이나 재시작처럼 맵을 이동할 때는 이전 맵 참조가 남지 않도록 UI를 정리하고 새 맵에서 다시 생성·바인딩한다.

## 6. 날짜 안내 화면

`WBP_DayIntro`에 `DayText`, `TargetText` Text Block을 둔다. 전체 화면 앵커를 사용하는 Overlay/Canvas를 사용한다.

`UpdateStatus` 함수(입력: `Casino Loop Status`)를 만든다.

```text
CurrentDay → Format Text "{Day}일차" → DayText.SetText
RequiredPayment → Format Text "오늘 목표: {Amount}원" → TargetText.SetText
```

Construct에서 `Get Game State → Cast to CasinoLoopGameState → LoopStatus → UpdateStatus`.
컨트롤러 `On UI Loop Status Updated`에서도 `Active Screen Widget → Cast WBP_DayIntro → UpdateStatus(Status)`를 호출한다.

위젯의 Delay로 다음 날을 시작하거나 스스로 닫지 않는다. GameMode의 `Day Intro Duration Seconds` 기본값은 3초다. 서버가 안내를 끝내면 자동으로 Playing으로 전환되고 화면이 제거된다. 그때부터 `Day Duration Seconds`가 계산된다. 시작 위치는 기존 `CasinoPaymentSpawn` 태그가 붙은 TargetPoint를 사용한다.

## 7. 오늘 납부 성공과 최종 결과

`WBP_DayPassed`는 “오늘 납부 성공” 정도의 안내 화면이다. `Result Duration Seconds`가 지나면 서버가 다음 날 안내로 넘어간다. 등록하지 않아도 다음 날 진행은 된다.

`WBP_RunResult`에는 결과 텍스트, 재시작 버튼, 메인메뉴 버튼을 둔다. `UpdateStatus(Casino Loop Status)`에서:

- Phase=Cleared → “클리어”
- Phase=GameOver → “실패”
- 그 외 → “결과 확인 중” (RPC와 GameState 복제의 도착 순서 차이를 고려)

날짜 안내처럼 Construct와 `On UI Loop Status Updated`에서 UpdateStatus를 호출한다.

재시작 버튼:

```text
UpdateStatus
 → Get Owning Player → Cast BP_FirstPersonPlayerController
 → Can Restart Casino Run
 → RestartButton.SetIsEnabled

RestartButton.OnClicked
 → Get Owning Player → Cast BP_FirstPersonPlayerController
 → Server Restart Casino Run
```

방장만 재시작할 수 있다. 서버에서도 소유자와 종료 상태를 검사하므로 클라이언트가 호출해도 다른 사람의 판을 재시작하지 않는다. 재시작은 연결과 EOS 방을 유지하는 seamless travel이다. 맵·게임기·캐릭터/ASC를 새로 만들고, 유지되는 PlayerState의 인벤토리·강화·납부·준비 상태는 클래스 기본값으로 초기화한다. 1일차부터 시작한다. 캐릭터 시작 잔액은 기존 캐릭터/GAS 초기화 설정을 따른다. 새로 추가하는 영구/판별 데이터가 있으면 이 초기화 정책도 함께 검토한다.

메인메뉴 버튼:

```text
OnClicked → Get Owning Player → Cast BP_FirstPersonPlayerController → Return to Main Menu
```

이 함수는 기존 EOS `LeaveRoom` 정리를 사용한다. 직접 Open Level만 호출하지 않는다. 방장이 나가면 방이 종료되므로 버튼 문구를 “방 종료 후 메인메뉴”로 하거나 확인창 뒤에 호출한다. 다른 참가자는 자기만 나간다. 네트워크/세션 정리 오류는 기존 Casino Online Subsystem의 OnError를 표시한다.

## 8. 테스트 순서

테스트용 GameMode에서 Day Intro=3, Day Duration=15, Payment Duration=15, Result Duration=2로 짧게 설정한다. 테스트 후 원래 값으로 복구한다.

1. 서버+클라이언트로 메뉴/로비에서 HUD가 안 뜨는지 확인한다.
2. 시작 후 같은 날짜/목표가 표시되고, 3초 동안 이동/게임 시작이 막히는지 확인한다.
3. 하루 진행에서는 기본 HUD와 이동이 돌아오는지 확인한다.
4. 블랙잭·포커·사다리·주사위·상점 등 서로 다른 UI를 연 채 하루를 끝낸다. 등록된 화면은 모두 닫히고 잔액+납부 패드만 남아야 한다.
5. 0원 제출, 시간 초과, 목표보다 큰 금액 입력, 잔액 부족을 확인한다.
6. 목표 충족 후 다음 날 안내, 마지막 날 클리어, 부족 시 실패 화면을 확인한다.
7. 클라이언트 재시작 버튼 비활성화, 방장 재시작 후 두 사람 모두 1일차·초기 잔액·초기 인벤토리인지 확인한다.
8. 클라이언트 나가기와 방장 방 종료를 각각 확인한다.

빌드/자동 테스트는 실제 두 PC의 EOS 접속, BP 디자인 및 서버 이동 후 UI 표시를 대체하지 않는다.
