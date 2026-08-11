using UnityEngine;

namespace Wirrwarr
{
    /// <summary>
    /// Waffen-System 1:1 aus dem Prototyp (real-game.tsx):
    /// Feuerraten, Bloom/Spread, Recoil-Kick, Tracer-Hooks.
    /// </summary>
    public class WeaponSystem : MonoBehaviour
    {
        public enum WeaponId { Dorn, Brecher, Richter }

        [System.Serializable]
        public class WeaponDef
        {
            public string name;
            public float fireRate;     // Sekunden zwischen Schüssen
            public int magazine;
            public float baseBloomAdd; // Bloom-Zuwachs pro Schuss (0..1)
            public float recoilKick;   // Pitch-Kick in Rad
            public Color tracerColor;
        }

        [Header("Prototyp-Werte — nicht anfassen ohne Playtest")]
        public WeaponDef dorn = new WeaponDef { name = "DORN", fireRate = 0.18f, magazine = 24, baseBloomAdd = 0.16f, recoilKick = 0.012f, tracerColor = new Color(0.13f, 1f, 0.33f) };
        public WeaponDef brecher = new WeaponDef { name = "BRECHER", fireRate = 0.9f, magazine = 8, baseBloomAdd = 0.3f, recoilKick = 0.03f, tracerColor = new Color(1f, 0.8f, 0.2f) };
        public WeaponDef richter = new WeaponDef { name = "RICHTER", fireRate = 1.1f, magazine = 5, baseBloomAdd = 0.3f, recoilKick = 0.04f, tracerColor = new Color(0.2f, 0.8f, 1f) };

        [Header("Balancing")]
        public float bloomMax = 1f;
        public float bloomDecayPerSec = 0.9f;
        public float spreadFactor = 0.07f;
        public float reloadTime = 1.2f;
        public float reloadTimeRichter = 1.6f;
        public float maxRange = 200f;

        public WeaponId current = WeaponId.Dorn;
        public int ammo { get; private set; }
        public float Bloom01 => _bloom;
        public bool IsReloading => _reloadTimer > 0f;

        public System.Action<RaycastHit, WeaponDef> OnHit;      // Impact-FX/Hook
        public System.Action<Vector3, Vector3, Color> OnTracer; // Tracer-FX-Hook

        Camera _cam;
        float _fireCd;
        float _bloom;
        float _reloadTimer;

        WeaponDef CurrentDef => current switch
        {
            WeaponId.Brecher => brecher,
            WeaponId.Richter => richter,
            _ => dorn
        };

        void Awake()
        {
            _cam = GetComponentInParent<Camera>() ?? Camera.main;
            ammo = CurrentDef.magazine;
        }

        void Update()
        {
            _fireCd = Mathf.Max(0f, _fireCd - Time.deltaTime);
            _bloom = Mathf.Max(0f, _bloom - bloomDecayPerSec * Time.deltaTime);

            if (_reloadTimer > 0f)
            {
                _reloadTimer -= Time.deltaTime;
                if (_reloadTimer <= 0f) ammo = CurrentDef.magazine;
                return;
            }

            if (Input.GetKeyDown(KeyCode.R) && ammo < CurrentDef.magazine) StartReload();
            if (Input.GetButton("Fire1")) TryFire();
        }

        public void StartReload()
        {
            if (ammo >= CurrentDef.magazine) return;
            _reloadTimer = current == WeaponId.Richter ? reloadTimeRichter : reloadTime;
        }

        public void SwitchWeapon(WeaponId id)
        {
            current = id;
            ammo = CurrentDef.magazine;
            _bloom = 0f;
        }

        void TryFire()
        {
            if (_fireCd > 0f || IsReloading || ammo <= 0) return;
            var def = CurrentDef;
            _fireCd = def.fireRate;
            ammo--;
            _bloom = Mathf.Min(bloomMax, _bloom + def.baseBloomAdd);

            // Spread wie im Prototyp: zufällige Offset-Raycast-Richtung
            Vector2 spread = new Vector2(Random.Range(-0.5f, 0.5f), Random.Range(-0.5f, 0.5f)) * _bloom * spreadFactor;
            Ray ray = _cam.ViewportPointToRay(new Vector3(0.5f + spread.x, 0.5f + spread.y, 0f));

            Vector3 muzzle = transform.position;
            if (Physics.Raycast(ray, out RaycastHit hit, maxRange))
            {
                OnTracer?.Invoke(muzzle, hit.point, def.tracerColor);
                OnHit?.Invoke(hit, def);
            }
            else
            {
                OnTracer?.Invoke(muzzle, ray.origin + ray.direction * maxRange, def.tracerColor);
            }

            // Recoil-Kick (Pitch + minimaler Aimpunch)
            var t = transform.root;
            t.Rotate(-def.recoilKick * Mathf.Rad2Deg, 0f, 0f, Space.Self);

            if (ammo == 0) StartReload();
        }
    }
}
