# Linux 커널 기반 PWM 팬 제어 드라이버

Jetson Orin Nano에서 온도에 따라 팬을 제어하는 **팀 프로젝트**입니다. 팀에서 센서 입력, 팬 구동, 사용자 입력 및 상태 표시 기능을 나누어 구현했으며, **제가 담당한 부분은 DC 모터 드라이버 구현입니다.**

이 문서는 직접 구현한 DC 모터 드라이버를 중심으로 설명합니다.

`C` · `Linux Kernel Module` · `Platform Driver` · `Device Tree Overlay` · `PWM` · `GPIO` · `ioctl` · `Jetson Orin Nano`

## 담당 구현

- Device Tree의 `compatible` 값으로 연결되는 플랫폼 드라이버 구현
- GPIO descriptor API와 PWM API를 이용한 모터 구동 제어
- misc 문자 장치 `/dev/mydev` 등록 및 `open`, `release`, `ioctl` 처리
- 사용자 공간에서 전달한 듀티비의 범위 검사와 PWM 적용
- 뮤텍스를 이용한 모터 제어의 동시 접근 보호
- 듀티비 0%, 파일 해제, 드라이버 제거 시 정지 처리

## 제어 흐름

```text
실제 응용 프로그램 (Application/fan_app.c)
    │ ioctl(MY_IOCTL_CMD_SET_PWM, &duty_percent)
    ▼
/dev/mydev → 듀티비 복사·검사 → 뮤텍스 잠금 → GPIO·PWM 설정 → DC 모터
```

`MY_IOCTL_CMD_SET_PWM`으로 32비트 부호 없는 정수형 듀티비를 전달합니다. 드라이버는 0~100% 범위를 검사하고, 1ms 주기(1kHz)에 맞춰 PWM 활성 시간을 계산합니다.

듀티비가 1~100%이면 IN1=1, IN2=0으로 방향을 설정하고 PWM 출력을 활성화합니다. 0%이면 PWM 출력을 비활성화하고 두 방향 신호를 0으로 설정합니다. PWM 적용에 실패하면 정지를 시도하고 오류를 반환합니다.

## 주요 파일

| 파일 | 역할 |
| --- | --- |
| `drivers/dc_motor/motor.c` | 플랫폼 드라이버, 장치 파일 연산, GPIO·PWM 제어 |
| `drivers/dc_motor/motor.h` | 장치별 상태 구조체와 PWM 주기 정의 |
| `drivers/dc_motor/motor_ioctl.h` | 사용자 공간과 공유하는 ioctl 명령 정의 |
| `drivers/dc_motor/motor-overlay.dts` | 모터 장치의 GPIO·PWM 자원 연결 |
| `drivers/dc_motor/apptest.c` | 모터 드라이버 단독 테스트용 코드 |
| `Application/fan_app.c` | 팀 프로젝트의 실제 팬 제어 응용 프로그램 |

# 동작
```
BMP180 (IIO) ──온도──▶            ┌─ AUTO   : 22℃ 이상 ON / 20℃ 미만 OFF (히스테리시스)
                       fan_app ───┤           22~32℃ 구간 듀티 40~100% 선형 증가
EC11 (poll)  ──이벤트─▶            ├─ MANUAL : 엔코더 회전으로 듀티 5%씩 조절
                                  └─ WAIT   : 롱프레스로 정지·대기
                         │
                         ├──ioctl──▶ DC Motor (PWM)
                         └──ioctl──▶ LED Bar (팬 단계 0~8)
```

## 모터 드라이버 단독 테스트

`drivers/dc_motor/apptest.c`는 명령행에서 듀티비를 전달해 모터 드라이버를 테스트하는 코드입니다. 팀 프로젝트의 실제 응용 프로그램은 `Application/fan_app.c`이며, `/dev/mydev`에 ioctl 요청을 보내 모터를 제어합니다.

드라이버와 Device Tree Overlay가 적용되어 `/dev/mydev`가 생성된 Jetson/Linux 환경에서 실행합니다.

```sh
cd drivers/dc_motor
make apptest APP_CC=gcc
sudo ./apptest 50
```

위 명령은 듀티비를 50%로 설정합니다. Enter를 누르면 앱이 장치 파일을 닫고, 드라이버의 `release` 처리에서 모터를 정지합니다.
