# 블랙잭 딜러 연출

## 사용 에셋

- 테이블: `/Game/Blackjack/BluePrint/BP_BlackjackTable`
- 딜러: `/Game/Blackjack/BluePrint/BP_NPC_BlackjackDealer`
- 딜러의 실제 부모: C++ `BlackjackDealerVisual` (일반 Actor)
- AnimBP: `/Game/Blackjack/BluePrint/Animation/Dealer/ABP_BlackjackDealer_WBG`
- 리타게팅 결과: `/Game/Blackjack/BluePrint/Animation/Dealer/Retargeted`

딜러 BP 이름은 기존 참조를 유지하기 위해 그대로다. 부모는 NPC가 아니므로 대화·상호작용 구체·이동·ASC를 사용하지 않는다. 기존 테이블의 딜러 자식 액터 자리에 계속 사용한다.

## 게임과 연출의 관계

서버가 카드와 정산을 결정하고, 한 장씩 배분할 때마다 각 클라이언트에 연출 이벤트를 보낸다. 각 화면에서 만드는 카드 액터는 복제하지 않는다. 테이블과 게임 상태가 복제된다.

딜러는 카드를 오른손에 붙이고 배분 몽타주를 재생한다. 기존 `AN_Dealer_DealCardRelease` 알림이 실행되면 카드가 해당 플레이어·핸드·카드 순서의 목적지로 이동한다. 알림이 실행되지 않아도 대체 타이머로 손에서 놓는다. 애니메이션 알림으로 보상을 지급하거나 카드를 추가하지 않는다.

현재 게임에서는 섞기 연출을 끄고 첫 카드를 바로 배분한다. 자리별 방향 전환과 순차 배분은 계속 사용한다. 카드 순서를 무작위로 섞는 서버 규칙은 연출 설정과 별개다. 라운드·하루가 초기화되면 배분 타이머와 연출을 중지하고 카드를 정리한다. 중간에 플레이어가 나갈 때는 진행 중 연출 취소와 전체 테이블 카드 정리를 구분한다.

## 조정할 곳

딜러 BP의 클래스 기본값 → `Blackjack | Dealer`:

- `Deal Montage`: 카드 배분 몽타주
- `Shuffle Montage`: 카드 가져오기 → 섞기 1 → 섞기 2 → 카드 상자에 넣기 몽타주
- `Deal Play Rate`: 배분 애니메이션 속도
- `Deal Socket Name`: 카드가 붙는 소켓 이름. 기본값은 `Socket_CardDeal_R`이며 소켓이 없으면 `hand_r` 본을 사용한다.
- `Card Hand Offset`: `hand_r`을 사용할 때 카드의 손 기준 위치·회전 보정

`Blackjack | Dealer | Facing`:

- `bFaceDealtSeat`: 플레이어에게 배분할 때 메시만 카드 목적지 쪽으로 회전한다. 딜러 카드·셔플에서는 기본 방향으로 복귀한다.
- `Seat Facing Blend Seconds`: 방향 전환 시간. 기본값 0.15초.
- `Max Seat Facing Yaw`: 기본 방향에서 돌아갈 수 있는 최대 각도. 기본값 좌우 60도.
- `Seat Facing Yaw Offsets`: 자리 0~3의 추가 각도 보정. 카드 목적지에서 계산한 방향에 더한다.

`Blackjack | Dealer | Shuffle`:

- `bShowShuffleDecks`: 셔플 카드 뭉치 표시 여부.
- `Shuffle Left/Right Deck Offset`: 양손 본 기준 카드 뭉치의 위치·회전·크기. 손 모양에 맞춰 조정한다.
- `Shuffle Card Back Material`, `Shuffle Back Texture`: 카드 뭉치 윗면에 사용할 카드 뒷면.
- `Shuffle Prop Take Attach Fraction`: 카드 가져오기 동작에서 손에 뭉치가 나타나는 시점.
- `Shuffle Prop In Box Detach Fraction`: 카드 넣기 동작에서 뭉치가 사라지는 시점.

카드 뭉치는 두 개의 얇은 상자와 뒷면 평면으로 구성된다. 기존 손 애니메이션을 따라 움직이며 개별 카드가 휘어지는 연출은 포함하지 않는다. 딜러 위치와 테이블·의자·카메라의 배치는 변경하지 않는다.

테이블 BP의 클래스 기본값 → 딜러 연출 설정:

- `bAnimateDealer`: 순차 연출 활성화
- `bAnimateDealerShuffle`: 선택적인 섞기 동작. 현재 기본값은 꺼짐이다. 이 값이 꺼져 있으면 섞기 동작과 대기 시간을 모두 건너뛴다.
- `DealerCardIntervalSeconds`: 한 장을 배분한 뒤 다음 동작까지의 간격
- `DealerShuffleSeconds`: 섞기 연출을 켰을 때만 사용하는 대기 시간. 현재 테이블은 0초다. 다시 켤 경우 섞기 몽타주 길이보다 길게 설정한다.

배분 속도나 몽타주 길이를 수동 변경하면 테이블 간격도 맞춘다. 배분 간격은 `몽타주 길이 / Deal Play Rate + 카드 이동 시간 + 여유 시간` 이상으로 잡는다. 현재 카드 이동 시간은 0.25초다.

## 보조 본 움직임

새 AnimBP의 AnimGraph에서 `breast_l`, `breast_r`, `butt_l`, `butt_r`에 AnimDynamics를 적용한다. 언리얼 마네킹에 없는 본의 움직임을 원본 애니메이션에서 복사하는 방식이 아니라, 리타게팅된 몸의 움직임에 물리 보조 움직임을 더한다.

각 AnimDynamics 노드에서 Alpha로 효과 강도, Angular Limits로 움직임 범위, Damping으로 흔들림이 가라앉는 정도를 조절한다. 기존 스켈레톤과 PhysicsAsset은 수정하지 않는다.

## 확인할 상황

1. 딜러 근처에서 대화 안내가 뜨지 않는지 확인한다.
2. 첫 베팅 뒤 카드 섞기와 순차 배분이 보이는지 확인한다.
3. 히트·더블·스플릿·딜러 추가 카드가 올바른 자리로 가는지 확인한다.
4. 딜러 숨김 카드는 공개 전 뒷면으로 보이는지 확인한다.
5. 다음 라운드와 하루 종료 때 이전 카드·손에 붙은 카드가 남지 않는지 확인한다.
6. 멀티에서 서버와 클라이언트에 카드가 중복으로 보이지 않는지 확인한다.

## 에디터 작업 도구

`CasinoDealerEditor` 플러그인은 에디터 전용 에셋 생성·마이그레이션 도구다. 런타임 딜러는 프로젝트 C++ 모듈에 있다. 플러그인의 `CasinoDealerSetup` 커맨드렛은 Inspect, Build, Migrate, Refine 모드를 지원한다. Refine은 기존 배분 호출에 자리 번호를 연결하고 셔플 소품을 설정한다. 원본 BP 백업은 `Saved/CasinoDealerSetup/Original`, 방향·소품 추가 전 백업은 `Saved/CasinoDealerSetup/BeforeRefine`에 보관한다. 디자이너에서 수동 조정한 뒤에는 생성 도구를 다시 실행하기 전에 변경 사항을 확인한다.
