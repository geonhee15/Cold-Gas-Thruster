// =====================================================
// 냉가스 리펄서 - 1단계 추력 측정 리그 마운트
// OpenSCAD 파라메트릭 설계
//
// 사용법:
//  1. 로드셀 도착하면 아래 [실측 필요] 값들을 실측치로 수정
//  2. part 변수를 바꿔가며 각 부품 렌더(F6) -> STL 내보내기
//  3. PETG, 레이어 0.2, 인필 40%, 퍼리미터 4 권장
//
// 부품 구성:
//  part = "base"    베이스 + 로드셀 포스트 (클램프 날개 포함)
//  part = "bracket" 노즐 브래킷 (로드셀 자유단에 볼트 고정)
//  part = "spacer"  로드셀 스페이서 2개 (고정단/자유단용)
//  part = "all"     조립 미리보기
// =====================================================

part = "all"; // [base, bracket, spacer, all]

// ---------- 로드셀 치수 [실측 필요] ----------
lc_len        = 81;    // 로드셀 전체 길이
lc_w          = 12.7;  // 폭
lc_h          = 12.7;  // 두께
lc_hole_d     = 4.3;   // M4 볼트 통과 구멍
lc_hole_pitch = 15;    // 같은 쪽 구멍 2개 간격 [실측 필요]
lc_hole_end   = 5;     // 끝면에서 첫 구멍까지 [실측 필요]

// ---------- 노즐 어셈블리 치수 [실측 필요] ----------
fit_d   = 15;   // 황동 피팅/캡너트 육각 대각 지름 (V홈 크기)
fit_len = 30;   // 브래킷이 감싸는 길이

// ---------- 베이스 ----------
base_l  = 150;
base_w  = 90;
base_t  = 6;
post_h  = 25;   // 포스트 높이 (로드셀 고정단이 이 위에 앉음)
post_l  = 25;   // 포스트 길이 (로드셀 고정단 구멍 2개 커버)
clamp_hole = 8; // 클램프/볼트 고정용 구멍

spacer_t = 3;   // 로드셀 스페이서 두께 (막대 휨 공간)

$fn = 48;
bolt = 4.3;

// ---------- 모듈 ----------

module base() {
    difference() {
        union() {
            // 바닥판
            cube([base_l, base_w, base_t]);
            // 로드셀 포스트: 판 끝쪽에 배치
            translate([15, base_w/2 - lc_w/2 - 5, 0])
                cube([post_l + 10, lc_w + 10, base_t + post_h]);
        }
        // 로드셀 고정 볼트 구멍 2개 (포스트 상면, 수직 관통)
        for (i = [0, 1])
            translate([20 + lc_hole_end + i*lc_hole_pitch,
                       base_w/2, -1])
                cylinder(d = bolt, h = base_t + post_h + 2);
        // 클램프/책상 고정 구멍 4개
        for (x = [12, base_l - 12], y = [12, base_w - 12])
            translate([x, y, -1]) cylinder(d = clamp_hole, h = base_t + 2);
        // 케이블타이 슬롯 (HX711 배선 정리)
        translate([base_l - 40, base_w/2 - 15, -1]) cube([4, 30, base_t + 2]);
    }
}

module spacer() {
    difference() {
        cube([post_l, lc_w, spacer_t]);
        for (i = [0, 1])
            translate([lc_hole_end + i*lc_hole_pitch - 5 + 10, lc_w/2, -1])
                cylinder(d = bolt, h = spacer_t + 2);
    }
}

module bracket() {
    br_l = fit_len;
    br_w = lc_w + 10;
    br_t = 8;
    difference() {
        union() {
            // 로드셀 자유단에 앉는 판
            cube([post_l, br_w, br_t]);
            // V홈 블록 (노즐 피팅이 눕는 자리, 분사 방향 = +Y)
            translate([0, -2, br_t])
                cube([br_l, br_w + 4, fit_d]);
        }
        // 로드셀 볼트 구멍 2개
        for (i = [0, 1])
            translate([lc_hole_end + i*lc_hole_pitch - 5 + 10, br_w/2, -1])
                cylinder(d = bolt, h = br_t + 2);
        // V홈 (90도, 피팅이 안착)
        translate([-1, br_w/2, br_t + fit_d])
            rotate([0, 90, 0])
                rotate([0, 0, 45])
                    cube([fit_d, fit_d, br_l + 2]);
        // 케이블타이 슬롯 2개
        for (x = [br_l*0.25, br_l*0.75])
            translate([x - 2, -3, br_t + 1])
                cube([4, br_w + 6, fit_d + 2]);
    }
}

module loadcell_dummy() {
    color("silver") cube([lc_len, lc_w, lc_h]);
}

// ---------- 렌더 ----------
if (part == "base") base();
if (part == "spacer") { spacer(); translate([post_l + 5, 0, 0]) spacer(); }
if (part == "bracket") bracket();
if (part == "all") {
    base();
    // 로드셀: 포스트 위에 수직으로 세움 (미리보기용, 대략 배치)
    translate([20, base_w/2 - lc_h/2, base_t + post_h + spacer_t])
        rotate([0, -90, 0]) rotate([0, 0, 90])
            translate([0, 0, -lc_w]) loadcell_dummy();
    // 브래킷: 로드셀 상단 자유단 위치 (대략 배치)
    translate([45, base_w/2 - (lc_w+10)/2, base_t + post_h + lc_len - 5])
        bracket();
}

// =====================================================
// 조립 노트
// - 로드셀 화살표(있으면)가 분사 반대 방향을 가리키게
// - 고정단: 베이스 포스트 + 스페이서 + 로드셀, M4x25 볼트 2개
// - 자유단: 로드셀 + 스페이서 + 브래킷, M4x20 볼트 2개
// - 캘리브레이션은 조립 전에 수평 상태로 (c500), 조립 후 tare만
// - 분사 방향(+Y)이 브래킷 V홈 축과 일치, 벽/사람 없는 쪽으로
// =====================================================
