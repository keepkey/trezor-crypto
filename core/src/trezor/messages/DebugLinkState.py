# Automatically generated by pb2py
# fmt: off
import protobuf as p

from .HDNodeType import HDNodeType


class DebugLinkState(p.MessageType):
    MESSAGE_WIRE_TYPE = 102

    def __init__(
        self,
        layout: bytes = None,
        pin: str = None,
        matrix: str = None,
        mnemonic_secret: bytes = None,
        node: HDNodeType = None,
        passphrase_protection: bool = None,
        reset_word: str = None,
        reset_entropy: bytes = None,
        recovery_fake_word: str = None,
        recovery_word_pos: int = None,
        reset_word_pos: int = None,
        mnemonic_type: int = None,
    ) -> None:
        self.layout = layout
        self.pin = pin
        self.matrix = matrix
        self.mnemonic_secret = mnemonic_secret
        self.node = node
        self.passphrase_protection = passphrase_protection
        self.reset_word = reset_word
        self.reset_entropy = reset_entropy
        self.recovery_fake_word = recovery_fake_word
        self.recovery_word_pos = recovery_word_pos
        self.reset_word_pos = reset_word_pos
        self.mnemonic_type = mnemonic_type

    @classmethod
    def get_fields(cls):
        return {
            1: ('layout', p.BytesType, 0),
            2: ('pin', p.UnicodeType, 0),
            3: ('matrix', p.UnicodeType, 0),
            4: ('mnemonic_secret', p.BytesType, 0),
            5: ('node', HDNodeType, 0),
            6: ('passphrase_protection', p.BoolType, 0),
            7: ('reset_word', p.UnicodeType, 0),
            8: ('reset_entropy', p.BytesType, 0),
            9: ('recovery_fake_word', p.UnicodeType, 0),
            10: ('recovery_word_pos', p.UVarintType, 0),
            11: ('reset_word_pos', p.UVarintType, 0),
            12: ('mnemonic_type', p.UVarintType, 0),
        }
