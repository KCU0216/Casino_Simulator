# UI 매니저 전환 가이드

## 코드 구조
PlayerController의 UIManager 컴포넌트가 UIRoot, 열린 UI 목록, 화면 상태와 맵 이동 정리를 관리한다.
컨트롤러의 기존 ShowInteractionUI / CloseInteractionUI는 호환용 전달 함수다.
UIScreen, UIRoot, ActiveScreenWidget도 기존 BP가 읽을 수 있도록 매니저 상태를 반영한다.
ScreenWidgetClasses 설정 위치와 기존 컨트롤러 BP 이벤트는 유지한다.

## 에디터 시작
에디터를 종료하고 전체 C++ 빌드를 완료한 뒤 다시 연다. 새 부모/컴포넌트 추가에는 Live Coding을 사용하지 않는다.
BP_FirstPersonPlayerController에 기본 UIManager 컴포넌트가 있는지 확인한다.

## 일반 WBP 부모
클래스 설정에서 직접 부모가 UserWidget인 상호작용 WBP를 CasinoManagedWidget으로 변경하고 컴파일한다.
예: DiceBetting, ThreeCardPokerBetting, Trade, BlackjackHUD, Shop.
PauseMenuWidget와 InventoryWidget는 C++에서 공통 부모를 상속하도록 변경했으므로 BP 부모는 그대로 유지한다.
전용 C++ 부모가 있는 다른 위젯은 직접 부모를 변경하지 않는다.
디자이너, 변수, 기존 그래프는 유지된다. 부모 교체 후 컴파일 오류를 먼저 확인한다.

## 열기
기존 생성/캐시 변수를 유지하는 방식:
Get Owning Player 또는 캐릭터의 Get Controller → Cast Casino PlayerController → Get UIManager.
매니저의 ShowInteractionUI(Widget=현재 위젯)를 호출한다.
Create Widget의 Owning Player는 같은 컨트롤러여야 한다.
매니저가 화면에 붙이고 마우스 및 GameAndUI 입력을 설정한다.
BP에서 추가로 Show Mouse Cursor나 Set Is Interaction UI Open을 설정할 필요는 없다.

생성까지 매니저에 맡기는 방식:
Get UIManager → OpenInteractionUI(WidgetClass=원하는 WBP).
반환값이 유효한 경우만 게임별 데이터 설정/후속 작업을 실행한다.
매니저는 클래스별로 인스턴스를 재사용한다. 여러 인스턴스가 필요하면 직접 Create Widget 후 ShowInteractionUI를 사용한다.
첫 OnOpened에 필요한 데이터가 있다면 Create → 변수 설정 → Show 순서를 사용한다.

## 열 때 초기화
공통 부모의 Event OnOpened는 닫았다가 다시 열 때마다 실행된다.
DiceBetting: InputValue=0, 선택 타입 기본값, 금액/선택 텍스트 갱신, 키패드 숨기기.
서버에 제출한 베팅과 정산 정보는 초기화하지 않는다.
Construct에는 고정 UI 설정만 두고, 반복 열기에 필요한 초기화는 OnOpened로 옮긴다.
Construct가 다시 호출될 수 있으므로 외부 델리게이트 중복 바인딩도 주의한다.

## 닫기
단순 화면 닫기: 공통 부모의 RequestClose 또는 매니저 CloseInteractionUI(Self).
머신/NPC에서 완전히 나가기: 기존 서버 이용 종료 요청 CloseCurrentInteraction을 먼저 호출하고 화면 닫기를 별도로 요청한다.
화면을 닫는 것만으로 서버 자리/베팅이 종료되는 것은 아니다.
OnClosed에서는 화면별 참조/델리게이트를 정리한다.
OnClosed에서 RequestClose를 다시 호출하거나 SetInputMode/마우스/Walking을 설정하지 않는다.
마지막 상호작용 UI를 닫으면 기존 입력 복구 함수를 사용한다.
납부/결과 상태에서는 게임 입력으로 복구하지 않는다.

## 별도 키패드
Dice의 Calculate처럼 따로 Create Widget/AddToViewport한 키패드는 별도 등록이 필요하다.
Owning Player를 지정하고 ShowInteractionUI로 열고 CloseInteractionUI로 닫는다.
부모 OnClosed에서 별도 키패드를 CloseInteractionUI로 정리한다.
디자이너에 포함된 자식 키패드는 부모와 같이 사라지므로 별도 등록하지 않는다.
입력 이벤트는 키패드가 새로 생성될 때 한 번만 연결하거나 명시적으로 해제 후 다시 연결한다.

## 테스트
1. Dice 입력 → 나가기 → 재입장: 입력 0으로 초기화.
2. Trade/Shop/Poker 열기·닫기: 마우스와 WASD 복구.
3. 메뉴와 인벤토리를 여러 번 열고 닫기: 중복 UI 없음.
4. 게임 UI + 별도 키패드를 켜 둔 채 하루 종료: 모두 제거되고 골드 HUD와 납부 UI 유지.
5. 납부/결과 중 상호작용 UI 열기 거부.
6. 호스트와 클라이언트에서 각각 테스트, 다음 날·재시작·로비 이동 후 다시 열기.
블랙잭 E/PrimaryInput 변경과 슬롯 내부 회전·정산 변경은 이번 작업에 포함하지 않는다.
