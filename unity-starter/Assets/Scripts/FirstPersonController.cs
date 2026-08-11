using UnityEngine;

namespace Wirrwarr
{
    /// <summary>
    /// 1:1-Port des Browser-Controllers (real-game.tsx).
    /// Zahlen sind die bewehrten Prototyp-Werte — nicht anfassen ohne Playtest.
    /// </summary>
    [RequireComponent(typeof(CharacterController))]
    public class FirstPersonController : MonoBehaviour
    {
        [Header("Bewegung (Prototyp-Werte)")]
        public float walkSpeed = 5.2f;
        public float sprintSpeed = 7.6f;
        public float crouchSpeed = 2.8f;
        public float proneSpeed = 1.4f;
        public float slideSpeed = 9.5f;
        public float slideDuration = 0.9f;
        public float gravity = -22f;
        public float jumpVelocity = 7.4f;

        [Header("Kamera (Prototyp-Werte)")]
        public float standHeight = 1.7f;
        public float crouchHeight = 1.15f;
        public float proneHeight = 0.55f;
        public float fovDefault = 75f;
        public float fovSprint = 81f;
        public float fovSlide = 84f;
        public float fovProne = 70f;

        [Header("Feel")]
        public float coyoteTime = 0.12f;
        public float jumpBufferTime = 0.12f;
        public float bobFrequency = 1.7f;
        public float bobAmplitude = 0.045f;
        public float landDipRecovery = 0.5f;

        public enum Stance { Stand, Crouch, Prone }
        public Stance stance { get; private set; } = Stance.Stand;
        public bool IsSliding => _slideTimer > 0f;
        public Vector3 Velocity => _velocity;

        CharacterController _cc;
        Camera _cam;
        Vector3 _velocity;
        float _coyote;
        float _jumpBuffer;
        float _slideTimer;
        float _bobPhase;
        float _landDip;
        float _currentHeight;

        void Awake()
        {
            _cc = GetComponent<CharacterController>();
            _cam = GetComponentInChildren<Camera>();
            _currentHeight = standHeight;
            if (_cam != null) _cam.fieldOfView = fovDefault;
        }

        void Update()
        {
            ReadInput();
            UpdateStance();
            UpdateGroundAndCoyote();
            UpdateJump();
            UpdateSlide();
            Move();
            UpdateCamera();
        }

        void ReadInput()
        {
            if (Input.GetKeyDown(KeyCode.C))
                stance = stance == Stance.Stand ? Stance.Crouch : Stance.Stand;
            if (Input.GetKeyDown(KeyCode.LeftControl))
                stance = stance == Stance.Prone ? Stance.Stand : Stance.Prone;

            if (Input.GetButtonDown("Jump")) _jumpBuffer = jumpBufferTime;
            else _jumpBuffer = Mathf.Max(0f, _jumpBuffer - Time.deltaTime);

            // Slide: Sprint + Crouch während Bewegung (Prototyp-Verhalten)
            if (IsSprinting() && _cc.isGrounded && Input.GetKeyDown(KeyCode.C) && _velocity.magnitude > 4f)
                _slideTimer = slideDuration;
        }

        bool IsSprinting() => Input.GetKey(KeyCode.LeftShift) && stance == Stance.Stand && GetMoveInput().magnitude > 0.1f;

        Vector2 GetMoveInput() => new Vector2(Input.GetAxisRaw("Horizontal"), Input.GetAxisRaw("Vertical"));

        void UpdateStance()
        {
            float target = stance switch
            {
                Stance.Crouch => crouchHeight,
                Stance.Prone => proneHeight,
                _ => standHeight
            };
            _currentHeight = Mathf.MoveTowards(_currentHeight, target, 6f * Time.deltaTime);
            _cc.height = _currentHeight;
            _cc.center = new Vector3(0, _currentHeight * 0.5f, 0);
        }

        void UpdateGroundAndCoyote()
        {
            if (_cc.isGrounded)
            {
                _coyote = coyoteTime;
                if (_velocity.y < -8f) _landDip = 0.25f; // Lande-Dip wie im Prototyp
                _velocity.y = -2f;
            }
            else _coyote = Mathf.Max(0f, _coyote - Time.deltaTime);

            _landDip = Mathf.Max(0f, _landDip - Time.deltaTime * landDipRecovery);
        }

        void UpdateJump()
        {
            if (_jumpBuffer > 0f && _coyote > 0f && stance == Stance.Stand && !IsSliding)
            {
                _velocity.y = jumpVelocity;
                _coyote = 0f;
                _jumpBuffer = 0f;
            }
        }

        void UpdateSlide()
        {
            _slideTimer = Mathf.Max(0f, _slideTimer - Time.deltaTime);
        }

        void Move()
        {
            Vector2 input = GetMoveInput();
            float speed = stance switch
            {
                Stance.Crouch => crouchSpeed,
                Stance.Prone => proneSpeed,
                _ => IsSprinting() ? sprintSpeed : walkSpeed
            };
            if (IsSliding) speed = slideSpeed * (_slideTimer / slideDuration + 0.3f);

            Vector3 planar = (transform.right * input.x + transform.forward * input.y) * speed;
            _velocity.x = Mathf.MoveTowards(_velocity.x, planar.x, 40f * Time.deltaTime);
            _velocity.z = Mathf.MoveTowards(_velocity.z, planar.z, 40f * Time.deltaTime);
            _velocity.y += gravity * Time.deltaTime;

            _cc.Move(_velocity * Time.deltaTime);

            if (_cc.isGrounded && planar.magnitude > 1f)
                _bobPhase += planar.magnitude * Time.deltaTime * bobFrequency;
        }

        void UpdateCamera()
        {
            if (_cam == null) return;

            float targetFov = IsSliding ? fovSlide
                : IsSprinting() ? fovSprint
                : stance == Stance.Prone ? fovProne
                : fovDefault;
            _cam.fieldOfView = Mathf.MoveTowards(_cam.fieldOfView, targetFov, 60f * Time.deltaTime);

            // Head-Bob + Lande-Dip (dezenter Prototyp-Wert)
            float bob = _cc.isGrounded ? Mathf.Sin(_bobPhase) * bobAmplitude : 0f;
            float eye = _currentHeight + bob - _landDip * 0.4f;
            _cam.transform.localPosition = Vector3.MoveTowards(
                _cam.transform.localPosition, new Vector3(0, eye, 0), 12f * Time.deltaTime);
        }
    }
}
