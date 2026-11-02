import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {FileBlob, PresentationFile} from 'file:///C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs';

const build=path.dirname(fileURLToPath(import.meta.url));
const workspace=path.resolve(build,'../..');
const source='C:/Users/user1/Desktop/CASINO_발표자료_20분_최종.pptx';
const p=await PresentationFile.importPptx(await FileBlob.load(source));
const orig=[...p.slides.items];
const gold='#EFCB80', white='#F5F3EB', muted='#B4BAB0', bg='#11130F';
function find(s,text){const sh=s.shapes.items.find(x=>String(x.text)===text);if(!sh)throw new Error('Missing text: '+text);return sh;}
function replace(s,a,b){find(s,a).text.replace(a,b);}
function textbox(s,text,left,top,width,height,size,color=white,bold=false,name='text'){
 const sh=s.shapes.add({geometry:'textbox',name,position:{left,top,width,height},fill:'none',line:{fill:'none',width:0}});
 sh.text=text;sh.text.style={typeface:'Malgun Gothic',fontSize:size,color,bold,insets:0,wrap:'word',verticalAlignment:'top'};return sh;
}
function blank(title,message){const s=p.slides.add({background:{fill:bg}});textbox(s,title,64,52,1136,65,44,white,true,'slide-title');textbox(s,message,64,131,1136,64,26,gold,false,'main-message');textbox(s,'00',1160,660,56,27,18,muted,false,'slide-number').text.alignment='right';return s;}
function table(s,values,widths,top,height,size=24){
 const t=s.tables.add({rows:values.length,columns:values[0].length,left:64,top,width:1152,height,columnWidths:widths,values});
 const all=t.cells.block({row:0,column:0,rowCount:values.length,columnCount:values[0].length});
 all.assign({fill:bg,textStyle:{typeface:'Malgun Gothic',fontSize:size,color:white},margins:{left:14,right:14,top:10,bottom:10},anchor:'center'});
 t.borders.assign({style:'solid',fill:'#333A2F',width:1});
 const head=t.cells.block({row:0,column:0,rowCount:1,columnCount:values[0].length});head.fill='#1B2119';head.textStyle.color=gold;head.textStyle.bold=true;
 return t;
}

// Supplied cover, artwork and existing slide layouts remain in place.
const overview=blank('게임 개요와 구성','개인별 자금 운용과 팀의 공동 납부를 연결한 4인 멀티플레이 게임입니다.');
table(overview,[
 ['구분','구현 내용'],
 ['플레이 목표','카지노와 채굴로 수입을 만들고, 매일 공동 납부 목표를 달성합니다.'],
 ['카지노 6종','블랙잭·쓰리카드 포커·홀짝·슬롯머신·사다리타기·경마'],
 ['협동 활동','채굴·공동 운반·수레·장비 강화·보석금 분담'],
 ['온라인과 소통','4인 기본 구성, EOS 로비·준비·음성 채팅'],
 ['제작 환경','Unreal Engine 5.7, C++·Blueprint']
],[230,922],218,390,24);
textbox(overview,'공통 자금과 하루 시간 안에서 각자의 선택이 팀의 결과에 반영됩니다.',64,627,1090,36,23,muted);

const interaction=orig[2].duplicate();
replace(interaction,'하루 진행 구조','공통 상호작용');
replace(interaction,'활동이 달라도 하루 마감과 납부 단계는 같습니다.','테이블·기계·NPC가 같은 탐지와 시작·종료 흐름을 사용합니다.');
for(const [a,b] of [['날짜 안내','탐지'],['활동','요청'],['종료 정리','서버 확인'],['공동 납부','이용'],['다음 날·종료','상태 복구']]) replace(interaction,a,b);
replace(interaction,'활동 시간','대상 선택과 점유');
replace(interaction,'채굴 / 카지노\n구매 / 방해 대응','주변 후보·시선 탐지\nE 입력으로 요청');
replace(interaction,'종료 시점','공통 시작·종료 처리');
replace(interaction,'같은 마감에 게임·점유·화면을 정리합니다.\n전원 제출 후 납부 목표 달성을 판단합니다.','서버가 가능 여부와 점유를 확인합니다.\n해제 시 카메라·이동·이용 상태를 복구합니다.');
replace(interaction,'메인 메뉴와 EOS 로비에서 입장·준비를 마친 뒤 하루를 시작합니다.','하루 종료에는 게임 정리와 점유 해제, 화면 전환을 공통 흐름으로 처리합니다.');

const loading=orig[6].duplicate();
replace(loading,'채광 업그레이드 상점','함께 시작하기 위한 로딩');
replace(loading,'업그레이드를 구매하면 채굴 능력과 레벨을 바로 변경합니다.','먼저 맵에 도착한 플레이어도 모두의 준비가 끝날 때까지 기다립니다.');
for(const [a,b] of [['Character의\n서버 구매 요청','월드·UI\n준비'],['개인 자금\n결제','전원 준비\n확인'],['GameplayEffect\n적용','스폰 위치\n적용'],['PlayerState\n강화 레벨 증가','시작 승인\n날짜 안내']])replace(loading,a,b);
replace(loading,'채광력 Power','1차 월드 준비');
replace(loading,'채광 위력을 강화합니다.\nPlayerState의 Power 레벨을 올립니다.','맵·로컬 플레이어·UI를 확인합니다.\n각 클라이언트가 준비를 알립니다.');
replace(loading,'채광 속도 Speed','2차 스폰 준비');
replace(loading,'채광 속도를 강화합니다.\nPlayerState의 Speed 레벨을 올립니다.','서버가 시작 위치를 배치합니다.\n각 클라이언트가 적용을 확인합니다.');
replace(loading,'가격은 현재 강화 레벨에 따라 증가합니다.','이전 로딩 응답은 제외하고, 시간 초과와 접속 종료를 구분합니다.');

const problems=blank('상태 경계와 문제 해결','반복 입력·하루 마감·로딩 응답이 겹치는 순간을 단계별로 처리합니다.');
table(problems,[
 ['상황','처리 방식','처리 결과'],
 ['중복 요청','진행 단계·제출 여부를 서버에서 확인합니다.','결제·납부·지급을\n한 번만 처리합니다.'],
 ['하루 마감','게임·타이머·점유·화면을 순서대로 정리합니다.','납부 단계의\n입력 상태를 유지합니다.'],
 ['연출·복제 순서','서버 판정과 화면 연출을 분리합니다.','판정 결과를 기준으로\n화면을 갱신합니다.'],
 ['이전 로딩 응답','로딩 식별자와 준비 상태를 검사합니다.','현재 시작 과정에만\n응답을 반영합니다.']
],[250,530,372],218,384,23);
textbox(problems,'콘텐츠별 규칙을 유지하면서, 공통 단계에서 자금과 이용 상태를 관리합니다.',64,627,1090,36,23,muted);

replace(orig[2],'같은 마감에 게임·점유·화면을 정리합니다.\n전원 제출 후 납부 목표 달성을 판단합니다.','같은 마감에 게임·점유·화면을 정리합니다.\n전원 제출 또는 납부 마감 후\n목표 달성을 판단합니다.');
replace(orig[3],'GameController가 플레이어의 요청과 게임별 진행을 연결합니다.','PlayerController가 입력과 서버 요청, 화면 전환을 연결합니다.');
replace(orig[4],'운반 부담은 함께 나누고, 판매금은 LastCarrier에게 지급합니다.','운반 부담은 함께 나누고, 판매금은 마지막 운반자에게 지급합니다.');
replace(orig[4],'광물을 운반한 뒤 판매합니다.','광물은 직접 또는 수레로\n운반한 뒤 판매합니다.');
replace(orig[4],'판매금은 LastCarrier 개인에게 지급합니다.','판매금은 마지막 운반자에게\n지급합니다.');
replace(orig[4],'고급 광물의 2인 필수 조건은 추가 확인이 필요합니다.','공동 운반과 수레로 운반 부담을 줄입니다.');
replace(orig[5],'구매는 물품 지급, 사용은 효과 적용으로 나눕니다.','구매는 물품 지급, 사용은 효과 적용으로 나눕니다.');
replace(orig[9],'중복 지급을 방지합니다.','적중 시 원금 포함 2배를 반환하며, 중복 지급을 방지합니다.');
replace(orig[10],'릴과 UI 표현을 담당합니다.\n실제 지급·내부 종료 연결은\n추가 검증 대상입니다.','릴 회전과 당첨 영역을 표현합니다.\n입력 금액과 계산 결과를\n화면에 연결합니다.');
replace(orig[11],'결과 확정과 경로 표현을 분리합니다.','도착지는 서버가 확정하고, 경로 종료 시 지급합니다.');
replace(orig[12],'구매 당시 배당·프로필을 보존하며, 당첨권은 직접 환전합니다.','서버 시각으로 경주를 재생하고, 구매 당시 배당으로 직접 환전합니다.');
replace(orig[15],'팀 복귀 비용이 당일 납부 자금과 경쟁합니다.','여러 팀원이 부분 납부하며,\n잔여 보석금까지만 처리합니다.');
replace(orig[16],'공동 납부\n남은 잔액은 다음 목표의 기준','공동 납부\n전날 목표·남은 잔액 반영');
replace(orig[16],'취객의 전방 차단도 이동·활동 시간을 사용합니다.','다음 목표는 전날 목표와 납부 후 팀 잔액 중 큰 값을 기준으로 계산합니다.');

// Current source formula: ceil(max(previous bill, post-payment balance) * day multiplier).
replace(orig[17],'한 팀의 하루 사례','한 팀의 하루와 상납금 계산');
replace(orig[17],'같은 납부 목표를 서로 다른 수익과 지출로 채웁니다.','설명용 사례: 둘째 날 4명 × 250, 목표 1,500을 납부합니다.');
replace(orig[17],'설명용 가정: 둘째 날 시작 4명 × 250 = 1,000, 기본 배율 1.5, 목표 1,500','다음 상납금 = 올림[max(전날 목표, 납부 후 팀 잔액) × 일차 배율]');
replace(orig[17],'잔액 900이면 다음 목표 1,350','납부 후 팀 잔액 900');
replace(orig[17],'잔액 1,200이면 다음 목표 1,800','셋째 날 목표 2,400 (배율 1.6)');
const formulaFoot=find(orig[17],'같은 납부액에서 수익이 300 더 있는 경우를 비교합니다. 실제 플레이 로그가 아닙니다.');
formulaFoot.text.replace(String(formulaFoot.text),'일차 배율 = 1.5 + max(0, 일차−2) × 0.1  /  첫날 상납금 400\n둘째 날 납부 후 셋째 날 목표를 계산한 설명용 사례이며, 실제 플레이 로그가 아닙니다.');
formulaFoot.position={left:64,top:653,width:1090,height:52};
replace(orig[18],'게임 시작 후 중도 참가·재접속·호스트 이전에는 제한이 있습니다.','호스트 이탈 시 방을 종료하고 메인 메뉴로 돌아갑니다.');

const end=orig[19];
for(const [a,b] of [
 ['구현 경계와 검증 과제','구현 결과'],
 ['결과와 지급, 하루 마감의 연결을 검증하겠습니다.','여섯 카지노 게임과 협동 활동을 하나의 멀티플레이 흐름으로 연결했습니다.'],
 ['추가 검증 항목','완성된 플레이 구성'],
 ['정보 보호','게임 콘텐츠'],['개인 잔액·납부액의 네트워크 비공개 보호','6종 카지노·채굴·공동 운반·성장'],
 ['하루 종료','온라인'],['슬롯 내부 연결과 진행 중 경마 중단','EOS 로비·음성 채팅·동시 시작'],
 ['경제·온라인','공통 시스템'],['채굴 단독 클리어, 배당·경제, 다중 PC 안정성','서버 판정·상호작용·HUD·설정·사운드'],
 ['실제 설정','플레이 기록'],['5일·7일 기간 차이와 최종 순위 구현','공동 납부·일별 기록·최종 결과'],
 ['개인의 수익·투자·손실은\n공동 납부 능력으로 모이고,\n남은 자금은 다음 날\n목표에도 영향을 줍니다.','개인의 선택이\n팀의 공동 납부로 이어지는\n멀티플레이 게임'],
 ['각 게임의 규칙이 같은 자금과 시간 안에서\n팀의 생존 결과를 만듭니다.','게임의 시작부터 결과 공유까지\n하나의 흐름으로 구성했습니다.']
])replace(end,a,b);

const order=[orig[0],overview,orig[1],orig[2],orig[3],interaction,...orig.slice(4,19),loading,problems,end];
order.forEach((s,i)=>s.moveTo(i));
if(order.length!==24||p.slides.items.length!==24)throw new Error('Expected 24 slides');
for(let i=0;i<order.length;i++){
 const num=order[i].shapes.items.find(s=>s.name==='slide-number')||find(order[i],'01');
 num.text.replace(String(num.text),String(i+1).padStart(2,'0'));
}

const notes=[
 '안녕하세요. Unreal Engine 5.7로 제작한 CASINO를 소개하겠습니다. 플레이어는 채굴과 카지노 게임으로 개인 자금을 마련하고, 하루가 끝나면 공동 상납 목표를 달성합니다. 각자의 소비와 투자, 손익이 팀의 생존에 어떻게 연결되는지 설명드리겠습니다.',
 '온라인 기본 구성은 4인입니다. 블랙잭, 쓰리카드 포커, 주사위 홀짝, 슬롯머신, 사다리타기, 경마의 여섯 콘텐츠를 제공합니다. 채굴과 공동 운반으로 수입을 만들고, 소비품과 장비 강화에 투자할 수 있습니다. EOS 로비에서 준비를 마치고 함께 입장하며 음성 채팅으로 협력합니다. 게임별 규칙을 공통 자금과 하루 진행 안에 연결한 구성이 핵심입니다.',
 '플레이어마다 자신의 잔액을 관리합니다. 베팅과 구매는 개인 잔액에서 지불하고, 하루 마감에는 각자가 제출한 금액을 합산해 팀의 성공 여부를 판단합니다. 운반이나 보석금을 함께 처리해도 개인 자금과 공동 목표는 구분됩니다. 다음 날 상납금은 전날의 필수 목표와 납부 후 팀 잔액 중 큰 값에 일차 배율을 적용합니다. 잔액이 줄어도 전날 목표가 계산의 하한으로 남습니다.',
 '하루는 날짜와 목표 안내, 자유 활동, 종료 정리, 공동 납부, 결과 판정 순서로 진행됩니다. 활동 중에는 채굴과 카지노, 구매와 NPC 대응을 선택합니다. 마감에는 진행 중인 활동과 점유, 화면을 정리하고 납부 단계로 이동합니다. 전원이 제출하거나 납부 시간이 끝나면 목표 달성 여부를 판정합니다. 납부에 성공하면 다음 날로 진행하고 마지막 날에는 최종 결과를 표시합니다.',
 'GameMode는 하루 단계와 상납 목표, 성공과 실패를 결정합니다. GameState는 공유 상태와 시간 기준, 결과를 복제합니다. PlayerController는 입력과 서버 요청, 카메라와 화면 전환을 연결합니다. 개인 속성은 Character의 Ability System이, 인벤토리와 강화 레벨은 PlayerState가 관리합니다. UIManager와 상호작용 세션, 게임별 Actor와 Component로 화면, 점유, 규칙의 책임을 나눴습니다. 인벤토리와 개인 납부액은 소유자 전용 복제이며 Currency 속성은 공통 복제입니다.',
 '주변의 상호작용 후보를 등록한 뒤 시선으로 대상을 선택합니다. E 입력이 들어오면 서버가 이용 가능 여부와 점유 상태를 확인합니다. 이용을 마치면 좌석과 카메라, 이동을 정리합니다. 날짜 안내나 납부 단계에서는 이동이 다시 풀리지 않도록 현재 단계를 함께 확인합니다. 공통 탐지와 시작·종료 흐름을 사용하면서 콘텐츠마다 다른 규칙을 유지합니다.',
 '광석을 타격해 광물을 만들고 직접 운반하거나 수레에 실어 판매 구역으로 옮깁니다. 여러 명이 함께 운반하면 무게 부담을 나눌 수 있습니다. 판매금은 마지막 운반자에게 지급하며 자동으로 균등 분배하지 않습니다. 팀원은 운반에 협력할지, 다른 광물을 채굴할지 선택합니다. 이 수입을 베팅이나 장비 강화, 당일 상납에 사용할 수 있습니다.',
 '담배와 주류를 구매하면 개인 잔액을 차감하고 PlayerState 인벤토리에 물품을 넣습니다. 상점과 인벤토리는 공통 아이템 데이터로 이름과 가격, 효과를 관리합니다. 서버는 거리와 수량, 잔액을 확인하며 지급이 실패하면 환불합니다. 숫자 슬롯 1·2로 소비품을 사용하면 아이템을 하나 소비하고 효과를 적용합니다. 게임 속 니코틴은 걷기에, 알코올은 점프에 영향을 줍니다.',
 '채광 업그레이드는 채굴 위력과 속도를 강화합니다. Character가 서버 구매 요청을 받고 상점 컴포넌트가 가격과 잔액을 확인합니다. 결제 후 GameplayEffect를 적용하고 PlayerState의 해당 강화 레벨을 올립니다. 소비품을 인벤토리에 보관하는 구매와 달리 채광 능력에 바로 반영됩니다. 현재 강화 레벨에 따라 가격도 증가합니다.',
 '블랙잭은 최대 네 좌석이 같은 테이블 라운드를 진행합니다. 첫 베팅 후 대기 시간을 두며 전원이 베팅하면 바로 시작합니다. 카드를 나눈 뒤 Hit, Stand, Double, Split과 보험을 처리하고 딜러 진행 후 정산합니다. 라운드 중에는 퇴장을 예약하고 판이 끝난 뒤 자리를 해제합니다. 규칙과 착석·카메라를 분리했으며 비공개 딜러의 실제 패는 서버에 보관합니다.',
 '쓰리카드 포커는 처음에 Ante와 선택한 사이드 베팅을 지불합니다. 세 장의 패를 확인한 뒤 Play로 계속할지 Fold로 포기할지 선택합니다. Play를 선택하면 추가 비용을 지불하고 딜러와 패를 비교합니다. 결정 시간을 넘기면 Fold로 처리해 라운드가 멈추지 않게 합니다. 최초 베팅과 패 확인 후 결제를 별도 단계로 관리합니다.',
 '주사위 홀짝은 두 주사위의 합이 홀수인지 짝수인지 선택하는 게임입니다. 서버가 결과를 확정하고 주사위 표현을 재생한 뒤 판정과 지급을 처리합니다. 적중하면 원금을 포함해 베팅액의 두 배를 반환합니다. 결과 표현과 지급 완료를 분리하고 같은 완료 신호가 반복돼도 중복 지급하지 않도록 진행 상태를 검사합니다.',
 '슬롯머신은 다섯 열과 세 행의 보드에서 열여섯 개 당첨선을 평가합니다. C++이 라인별 당첨과 중복을 정리하고 DataTable의 심볼 및 결과 배율로 지급액을 계산합니다. Blueprint는 릴 회전과 UI, 당첨 영역 표현을 연결합니다. 평가 규칙과 배당 데이터, 시각 표현의 책임을 나눈 구조입니다.',
 '사다리타기는 줄과 줄 개수를 선택하고 베팅금을 입력합니다. 서버가 도착지를 먼저 정한 뒤 그 결과에 맞는 경로를 생성해 재생합니다. 당첨금은 구슬이 도착하는 경로 종료 시점에 지급합니다. 결과 결정과 경로 표현, 지급과 점유 해제를 별도로 관리합니다.',
 '경마는 입장과 베팅, 경주, 퇴장, 종료 순서로 진행됩니다. 서버가 주행 계획과 기준 시각을 공유하면 각 클라이언트가 경주 이동을 재생하고 서버가 최종 순위를 확정합니다. 마권에는 구매 당시 참가자 정보와 배당을 보존합니다. 적중한 마권은 직접 환전해야 개인 잔액에 반영되며 환전한 마권은 제거합니다.',
 '도둑은 개인 자금을 훔치는 NPC입니다. 돈을 보유한 상태에서 유효 타격을 받으면 도주로 전환하고, 도주 중 다시 타격하면 보유한 돈을 그 타격자에게 지급합니다. 원래 돈을 잃은 사람에게 자동으로 돌려주는 방식은 아닙니다. 플레이어는 추격으로 자금을 회수할지 다른 수익 활동을 계속할지 판단합니다. 계단 장면은 도주 중인 모습입니다.',
 '취객은 시야에 들어온 플레이어를 타깃으로 지정하고 진행 방향을 막아 이동을 방해합니다. 화면은 이동 모습과 플레이어 전방을 막는 모습을 보여 줍니다. 플레이어는 우회하거나 방해에 대응하면서 목적지로 이동합니다. 이동에 시간이 더 들면 같은 하루 안에서 채굴이나 카지노에 사용할 시간도 줄어듭니다.',
 '경찰은 플레이어를 추격하고 체포한 뒤 감옥으로 이동시킵니다. 감옥 안팎의 ATM이 보석금 상태를 공유하며 여러 팀원이 여러 차례 부분 납부할 수 있습니다. 납부는 남은 보석금 범위까지만 처리합니다. 팀원의 복귀를 돕기 위한 지출도 개인 자금에서 나오므로 당일 상납 여력과 연결됩니다.',
 '채굴 수입, 카지노 손익, 소비품과 장비 강화는 같은 개인 잔액을 사용합니다. 운반과 추격, 경주와 환전은 같은 하루 시간을 사용합니다. 협력으로 수입이나 활동 기회를 만들고, 각자가 얼마를 상납할지는 개인 잔액에서 결정합니다. 다음 날 목표는 전날 필수 목표와 납부 후 팀 잔액 중 큰 값에 배율을 적용합니다. 잔액이 줄어도 목표 계산이 초기화되지 않는 구조입니다.',
 '이 표는 실제 로그가 아닌 설명용 사례입니다. 네 명이 각자 250에서 시작하고 오늘 목표가 1,500이라고 가정합니다. A는 채굴 800과 강화 지출 100을 거쳐 600을, B는 500을, C는 400을 납부하고 D는 0을 제출합니다. 합계 1,500을 달성한 뒤 팀 잔액은 900입니다. 다음 목표는 max(전날 필수 목표 1,500, 납부 후 잔액 900)에 다음 일차 배율 1.5를 곱해 올림한 2,250입니다. 잔액이 1,200이어도 두 값 중 큰 수는 1,500이므로 같은 2,250입니다. 해당 일차 d의 배율은 기본 배율 + max(0,d−2) × 일별 증가량입니다. 둘째 날부터 기본 배율을 적용하고 셋째 날부터 일별 증가량이 누적됩니다. 첫날은 별도 지정 금액을 사용합니다. C++ 기본값은 첫날 100, 기본 배율 1.5, 일별 증가량 0이며 실제 에셋 설정과 설명용 가정은 구분합니다.',
 'EOS 기반 리슨 서버에서 방 생성과 참가, 준비, 맵 이동을 처리합니다. 서버가 공통 단계와 결과, 점유 상태를 관리하고 클라이언트는 복제 상태로 화면을 갱신합니다. 음성 채팅은 누르고 말하기와 개별 음량, 음소거를 지원합니다. UIManager가 HUD와 상호작용, 모달을 나누고 입력 모드를 전환합니다. 화면 품질과 음량, 감도를 저장하며 ID 기반 사운드 목록으로 UI·공간 효과음·BGM을 관리합니다. 호스트 이전은 지원하지 않으며 호스트가 이탈하면 방을 종료하고 메뉴로 돌아갑니다.',
 '맵에 먼저 도착한 플레이어가 혼자 하루를 시작하지 않도록 두 단계로 준비를 확인합니다. 첫 단계에서 각 클라이언트가 월드와 로컬 플레이어, UI 준비를 알립니다. 모두 준비되면 서버가 시작 위치를 배치하고 각 플레이어가 위치 적용을 확인합니다. 두 단계가 끝나야 날짜 안내를 시작합니다. 로딩 식별자로 이전 응답을 제외하고 시간 초과와 접속 종료도 구분합니다.',
 '여러 시스템이 겹치는 지점에서 진행 단계와 소유 상태를 명확히 관리했습니다. 반복 요청은 단계와 제출 여부를 검사해 결제와 납부를 한 번만 처리합니다. 하루 마감에는 타이머와 보상, 채굴과 점유, 화면을 순서대로 정리합니다. 판정과 연출은 서버 결과를 기준으로 연결합니다. 로딩 응답은 현재 식별자와 준비 상태를 확인합니다. 이 표는 각 상황에 대한 구현상 방어 처리 사례를 설명합니다.',
 'CASINO는 여섯 카지노 게임과 채굴, 공동 운반, 장비 강화와 보석금 분담을 하나의 멀티플레이 흐름으로 연결했습니다. 공통 자금과 하루 시간 안에서 개인의 판단이 공동 상납과 다음 날 목표에 반영됩니다. 로비 입장과 동시 시작, 게임별 판정과 화면 전환, 하루 종료와 일별 기록, 최종 결과까지 함께 구성했습니다. 이상으로 발표를 마치겠습니다. 감사합니다.'
];
const durations=[25,40,50,50,80,55,50,40,35,55,45,35,40,35,50,35,30,45,60,80,70,60,65,40];
notes[19]='이 표는 실제 로그가 아닌 설명용 사례입니다. 둘째 날 네 명이 각자 250에서 시작하고 오늘 목표가 1,500이라고 가정합니다. A는 채굴 800과 강화 지출 100을 거쳐 600을, B는 500을, C는 400을 납부하고 D는 0을 제출합니다. 합계 1,500을 달성한 뒤 팀 잔액은 900입니다. 셋째 날 목표는 max(전날 필수 목표 1,500, 납부 후 잔액 900)에 셋째 날 배율 1.6을 곱해 올림한 2,400입니다. 잔액이 1,200이어도 두 값 중 큰 수는 1,500이므로 같은 2,400입니다. 해당 일차 d의 배율은 1.5 + max(0,d−2) × 0.1입니다. 둘째 날 1.5, 셋째 날 1.6, 넷째 날 1.7, 다섯째 날 1.8로 증가합니다. 첫날은 별도 지정 금액 400을 사용합니다. 납부 후 잔액이 계속 전날 목표 이하라면 최소 목표의 예시는 1일400, 2일600, 3일960, 4일1,632, 5일2,938입니다. 실제 목표는 납부 후 팀 잔액이 더 크면 그 잔액을 기준으로 높아집니다. 이 값은 프로젝트 기본 게임모드 Blueprint의 클래스 기본 객체 설정을 기준으로 설명합니다.';
let elapsed=0;
function time(n){return String(Math.floor(n/60)).padStart(2,'0')+':'+String(n%60).padStart(2,'0');}
for(let i=0;i<order.length;i++){
 const refs=i===19?'프로젝트 Source/casino_simulator/casino_simulatorGameMode.cpp:567–589, 846–850 및 .h:82–89. Config/DefaultEngine.ini:75, Content/FirstPerson/Blueprints/BP_FirstPersonGameMode.uasset의 CDO: First Day Payment=400, Daily Payment Multiplier Increase Per Day=0.1, 기본 배수1.5 상속. 활성 맵 또는 런타임 재정의·제출 빌드 설정까지 대조한 것은 아닙니다.':'사용자 제공 CASINO_발표자료_20분_최종.pptx 및 Casino_Simulator_최종발표.pptx의 해당 기능 설명·프로젝트 화면.';
 order[i].speakerNotes.text=`발표 구간 ${time(elapsed)}–${time(elapsed+durations[i])}\n\n${notes[i]}\n\n출처(발표 시 읽지 않음): ${refs}`;
 elapsed+=durations[i];
}
await fs.writeFile(path.join(build,'final-inspect.ndjson'),(await p.inspect({kind:'slide,textbox,shape,table,notes',maxChars:200000})).ndjson);
for(let i=0;i<order.length;i++){
 const png=await order[i].export({format:'png',scale:1});
 await fs.writeFile(path.join(build,`final-${String(i+1).padStart(2,'0')}.png`),new Uint8Array(await png.arrayBuffer()));
}
const candidate=path.join(build,'candidate.pptx');
await (await PresentationFile.exportPptx(p)).save(candidate);
console.log(JSON.stringify({slides:order.length,duration:elapsed,candidate}));
const skill='C:/Users/user1/.codex/plugins/cache/openai-primary-runtime/presentations/26.1013.11914/skills/presentations';
process.env.RUNTIME_NODE_MODULES='C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules';
const {finalizePresentation}=await import(pathToFileURL(path.join(skill,'container_tools/artifact_tool_utils.mjs')).href);
const final=path.join(workspace,'output','통합발표자료','CASINO_최종발표_통합본.pptx');
await fs.mkdir(path.dirname(final),{recursive:true});
const originalLink=fs.link;
fs.link=async(src,dest)=>{try{return await originalLink(src,dest);}catch(error){if(error.code!=='EPERM'||dest!==final)throw error;await fs.copyFile(src,dest,1);}};
const result=await finalizePresentation({workspaceDir:workspace,candidatePath:candidate,finalPath:final,
 pythonExecutable:'C:/Users/user1/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
 integrityValidatorPath:path.join(skill,'container_tools/inspect_presentation_package_integrity.py'),
 layoutValidatorPath:path.join(skill,'container_tools/inspect_presentation_layout_geometry.py'),
 layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-bullet-geometry','--validate-heading-fit',...[2,5,20,23].flatMap(n=>['--require-native-table-slide',String(n)])],
 explicitTotalSlideCount:24,requiredNativeTableOwnerSlides:[2,5,20,23],
 fontPolicy:{basis:'reference',families:['Malgun Gothic'],referencePath:source,referenceSha256:crypto.createHash('sha256').update(await fs.readFile(source)).digest('hex')},
 verifyArtifactToolImport:true,receiptPath:path.join(build,'validation.json')});
console.log(JSON.stringify(result));
