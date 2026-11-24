import hmac
import hashlib

def compute_hmac_sha256(payload_bytes: bytes, secret_key: str) -> str:
    """Computes an HMAC-SHA256 signature for the given byte payload."""
    key_bytes = secret_key.encode("utf-8")
    return hmac.new(key_bytes, payload_bytes, hashlib.sha256).hexdigest()

def verify_hmac_sha256(payload_bytes: bytes, signature: str, secret_key: str) -> bool:
    """
    Verifies that the provided HMAC-SHA256 signature matches the payload.
    Uses constant-time comparison to prevent timing attacks.
    """
    if not signature:
        return False
    expected = compute_hmac_sha256(payload_bytes, secret_key)
    return hmac.compare_digest(expected.lower(), signature.strip().lower())

def calculate_sha256(data: bytes) -> str:
    """Calculates standard SHA-256 hash of byte data for tamper-proof evidence."""
    return hashlib.sha256(data).hexdigest()
