import json
import math
import sys
import os
import uuid
import ctypes

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QCheckBox, QTextEdit, QFileDialog,
    QStatusBar, QGroupBox, QComboBox, QMessageBox
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer, QSettings
from PyQt6.QtGui import QFont, QPalette, QColor, QIcon

try:
    import pyperclip
    PYPERCLIP_AVAILABLE = True
except ImportError:
    PYPERCLIP_AVAILABLE = False

def resource_path(relative):
    base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, relative)

THEMES = {
    "dark": {
        "window":           QColor(37, 37, 37),
        "window_text":      QColor(220, 220, 220),
        "base":             QColor(30,  30,  30),
        "alternate_base":   QColor(45,  45,  48),
        "text":             QColor(220, 220, 220),
        "button":           QColor(46, 46, 46),
        "button_text":      QColor(220, 220, 220),
        "highlight":        QColor(0,   120, 215),
        "highlighted_text": QColor(255, 255, 255),
        "tooltip_base":     QColor(50,  50,  53),
        "tooltip_text":     QColor(220, 220, 220),
        "placeholder":      QColor(130, 130, 130),

        "ok":               "#7dcea0",
        "error":            "#e74c3c",
        "dim":              "#888888",
        "theme_btn_label":  "Light",
    },
    "light": {
        "window":           QColor(240, 240, 240),
        "window_text":      QColor(30,  30,  30),
        "base":             QColor(255, 255, 255),
        "alternate_base":   QColor(233, 233, 233),
        "text":             QColor(30,  30,  30),
        "button":           QColor(220, 220, 220),
        "button_text":      QColor(30,  30,  30),
        "highlight":        QColor(0,   120, 215),
        "highlighted_text": QColor(255, 255, 255),
        "tooltip_base":     QColor(255, 255, 220),
        "tooltip_text":     QColor(30,  30,  30),
        "placeholder":      QColor(140, 140, 140),

        "ok":               "#1a7a40",
        "error":            "#c0392b",
        "dim":              "#777777",
        "theme_btn_label":  "Dark",
    },
}

_SETTINGS_ORG = "Json2AnimBP"
_SETTINGS_APP = "Json2AnimBP"
_SETTINGS_KEY = "theme"
_SETTINGS_DEFAULT = "dark"

def load_theme_preference() -> str:
    s = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
    return s.value(_SETTINGS_KEY, _SETTINGS_DEFAULT)

def save_theme_preference(name: str) -> None:
    s = QSettings(_SETTINGS_ORG, _SETTINGS_APP)
    s.setValue(_SETTINGS_KEY, name)

def build_palette(name: str) -> QPalette:
    c = THEMES[name]
    p = QPalette()
    p.setColor(QPalette.ColorRole.Window,          c["window"])
    p.setColor(QPalette.ColorRole.WindowText,      c["window_text"])
    p.setColor(QPalette.ColorRole.Base,            c["base"])
    p.setColor(QPalette.ColorRole.AlternateBase,   c["alternate_base"])
    p.setColor(QPalette.ColorRole.Text,            c["text"])
    p.setColor(QPalette.ColorRole.Button,          c["button"])
    p.setColor(QPalette.ColorRole.ButtonText,      c["button_text"])
    p.setColor(QPalette.ColorRole.Highlight,       c["highlight"])
    p.setColor(QPalette.ColorRole.HighlightedText, c["highlighted_text"])
    p.setColor(QPalette.ColorRole.ToolTipBase,     c["tooltip_base"])
    p.setColor(QPalette.ColorRole.ToolTipText,     c["tooltip_text"])
    p.setColor(QPalette.ColorRole.PlaceholderText, c["placeholder"])
    return p

def format_float(val):
    return "{0:.6f}".format(float(val))

def format_curve_keys(curve):
    if curve and "EditorCurveData" in curve and "Keys" in curve["EditorCurveData"]:
        keys = curve["EditorCurveData"]["Keys"]
        if keys:
            key_strings = []
            for key in keys:
                t = format_float(key.get("Time", 0))
                v = format_float(key.get("Value", 0))
                key_strings.append(f"(Time={t},Value={v})")
            return "EditorCurveData=(Keys=({0}))".format(",".join(key_strings))
    return None

def format_limits(limits, limit_type):
    if limits:
        formatted = []
        for limit in limits:
            r = format_float(limit.get("Radius", 0))
            l = format_float(limit.get("Length", 0))
            bone = limit.get("DrivingBone", {}).get("BoneName", "")
            ol = limit.get("OffsetLocation", {})
            or_ = limit.get("OffsetRotation", {})
            formatted.append(
                f'(Radius={r},Length={l},DrivingBone=(BoneName="{bone}"),'
                f'OffsetLocation=(X={format_float(ol.get("X",0))},Y={format_float(ol.get("Y",0))},Z={format_float(ol.get("Z",0))}),'
                f'OffsetRotation=(Pitch={format_float(or_.get("Pitch",0))},Yaw={format_float(or_.get("Yaw",0))},Roll={format_float(or_.get("Roll",0))}))'
            )
        return f"{limit_type}=({','.join(formatted)})"
    return None

def format_limits_data_asset(asset):
    if asset and "ObjectPath" in asset:
        path = asset["ObjectPath"].replace(".0", "")
        object_name = asset.get("ObjectName", "")
        short_name = object_name.split("''")[-2] if "''" in object_name else object_name
        return (
            "LimitsDataAsset=\"/Script/KawaiiPhysics.KawaiiPhysicsLimitsDataAsset''"
            + path + "." + short_name + "'\"'"
        )
    return None

def _new_guid() -> str:
    return uuid.uuid4().hex.upper()

def _pin_lines(in_guid: str, out_guid: str,
               prev_key: str, prev_out_guid: str,
               next_key: str, next_in_guid: str) -> str:
    in_linked = f"LinkedTo=({prev_key} {prev_out_guid},)," if prev_key and prev_out_guid else ""
    out_linked = f"LinkedTo=({next_key} {next_in_guid},),"  if next_key and next_in_guid  else ""
    lines = f'   CustomProperties Pin (PinId={in_guid},PinName="ComponentPose",'
    lines += f'Direction="EGPD_Input",PinType.PinCategory="",{in_linked})\n'
    lines += f'   CustomProperties Pin (PinId={out_guid},PinName="Pose",'
    lines += f'Direction="EGPD_Output",PinType.PinCategory="",{out_linked})\n'
    return lines

def _node_pos(index):
    return math.floor(index / 10) * 255, (index % 10) * 144

def format_kawaii_physics_node(key, node, index,
                                pin_in_guid="", pin_out_guid="",
                                prev_key="", prev_out_guid="",
                                next_key="", next_in_guid=""):
    root_bone = node.get("RootBone", {}).get("BoneName", "")
    dummy_bone_length = format_float(node.get("DummyBoneLength", 0))
    bone_axis = node.get("BoneForwardAxis", "").split(":")[-1]
    comptype = node.get("BoneConstraintGlobalComplianceType", "").split(":")[-1]
    tpdist = format_float(node.get("TeleportDistanceThreshold", 0))
    tprotate = format_float(node.get("TeleportRotationThreshold", 0))

    ps = node.get("PhysicsSettings", {})
    physics_string = ""
    if ps:
        physics_string = (
            f"Damping={format_float(ps.get('Damping',0))},"
            f"Stiffness={format_float(ps.get('Stiffness',0))},"
            f"WorldDampingLocation={format_float(ps.get('WorldDampingLocation',0))},"
            f"WorldDampingRotation={format_float(ps.get('WorldDampingRotation',0))},"
            f"Radius={format_float(ps.get('Radius',0))},"
            f"LimitAngle={format_float(ps.get('LimitAngle',0))}"
        )

    curve_parts = []
    for curve_name in [
        "DampingCurveData", "StiffnessCurveData",
        "WorldDampingLocationCurveData", "WorldDampingRotationCurveData",
        "RadiusCurveData", "LimitAngleCurveData",
        "LimitLinearCurveData", "GravityCurveData",
    ]:
        if curve_name in node and node[curve_name]:
            curve_str = format_curve_keys(node[curve_name])
            if curve_str:
                curve_parts.append(f"{curve_name}=({curve_str})")

    for limit_type in ["CapsuleLimits", "BoxLimits", "PlanarLimits", "SphericalLimits"]:
        if limit_type in node and node[limit_type]:
            limit_str = format_limits(node[limit_type], limit_type)
            if limit_str:
                curve_parts.append(limit_str)

    exclude_part = ""
    exclude_bones = node.get("ExcludeBones", [])
    if exclude_bones:
        ex = [f'(BoneName="{b.get("BoneName","")}")' for b in exclude_bones]
        exclude_part = f",ExcludeBones=({','.join(ex)})"

    if "LimitsDataAsset" in node and node["LimitsDataAsset"]:
        lda = format_limits_data_asset(node["LimitsDataAsset"])
        if lda:
            curve_parts.append(lda)

    extra = "," + ",".join(curve_parts) if curve_parts else ""
    posX, posY = _node_pos(index)

    out = f'Begin Object Class=/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics Name="{key}"\n'
    out += (f'   Node=(RootBone=(BoneName="{root_bone}"){exclude_part},DummyBoneLength={dummy_bone_length},'
            f'BoneForwardAxis={bone_axis},TeleportDistanceThreshold={tpdist},TeleportRotationThreshold={tprotate},'
            f'BoneConstraintGlobalComplianceType={comptype},PhysicsSettings=({physics_string}){extra})\n')
    out += '   ShowPinForProperties(0)=(PropertyName="ComponentPose",bShowPin=True)\n'
    out += '   ShowPinForProperties(1)=(PropertyName="bAlphaBoolEnabled",bShowPin=True)\n'
    out += '   ShowPinForProperties(2)=(PropertyName="Alpha",bShowPin=True)\n'
    out += '   ShowPinForProperties(3)=(PropertyName="AlphaCurveName",bShowPin=True)\n'
    if pin_in_guid and pin_out_guid:
        out += _pin_lines(pin_in_guid, pin_out_guid, prev_key, prev_out_guid, next_key, next_in_guid)
    out += f'   NodePosX={posX}\n'
    out += f'   NodePosY={posY}\n'
    out += 'End Object\n'
    return out

def format_modify_bone_node(key, node, index,
                             pin_in_guid="", pin_out_guid="",
                             prev_key="", prev_out_guid="",
                             next_key="", next_in_guid=""):
    bone = node.get("BoneToModify", {}).get("BoneName", "")
    t = node.get("Translation", {})
    r = node.get("Rotation", {})
    s = node.get("Scale", {})
    tmode = node.get("TranslationMode", "").split("::")[-1]
    rmode = node.get("RotationMode", "").split("::")[-1]
    smode = node.get("ScaleMode", "").split("::")[-1]
    tspace = node.get("TranslationSpace", "").split("::")[-1]
    rspace = node.get("RotationSpace", "").split("::")[-1]
    sspace = node.get("ScaleSpace", "").split("::")[-1]
    posX, posY = _node_pos(index)

    out = f'Begin Object Class=/Script/AnimGraph.AnimGraphNode_ModifyBone Name="{key}"\n'
    out += (f'   Node=(BoneToModify=(BoneName="{bone}"),'
            f'Translation=(X={format_float(t.get("X",0))},Y={format_float(t.get("Y",0))},Z={format_float(t.get("Z",0))}),'
            f'Rotation=(Pitch={format_float(r.get("Pitch",0))},Yaw={format_float(r.get("Yaw",0))},Roll={format_float(r.get("Roll",0))}),'
            f'Scale=(X={format_float(s.get("X",0))},Y={format_float(s.get("Y",0))},Z={format_float(s.get("Z",0))}),'
            f'TranslationMode={tmode},RotationMode={rmode},ScaleMode={smode},'
            f'TranslationSpace={tspace},RotationSpace={rspace},ScaleSpace={sspace})\n')
    out += '   ShowPinForProperties(0)=(PropertyName="ComponentPose",bShowPin=True)\n'
    out += '   ShowPinForProperties(1)=(PropertyName="bAlphaBoolEnabled",bShowPin=True)\n'
    out += '   ShowPinForProperties(2)=(PropertyName="Alpha",bShowPin=True)\n'
    out += '   ShowPinForProperties(3)=(PropertyName="AlphaCurveName",bShowPin=True)\n'
    if pin_in_guid and pin_out_guid:
        out += _pin_lines(pin_in_guid, pin_out_guid, prev_key, prev_out_guid, next_key, next_in_guid)
    out += f'   NodePosX={posX}\n'
    out += f'   NodePosY={posY}\n'
    out += 'End Object\n'
    return out

def format_constraint_node(key, node, index,
                            pin_in_guid="", pin_out_guid="",
                            prev_key="", prev_out_guid="",
                            next_key="", next_in_guid=""):
    bone = node.get("BoneToModify", {}).get("BoneName", "")
    setup_entries = []
    for constraint in node.get("ConstraintSetup", []):
        target_bone = constraint.get("TargetBone", {}).get("BoneName", "")
        offset = constraint.get("OffsetOption", "").split("::")[-1]
        ttype = constraint.get("TransformType", "").split("::")[-1]
        per_axis = constraint.get("PerAxis", {})
        setup_entries.append(
            f'(TargetBone=(BoneName="{target_bone}"),OffsetOption={offset},'
            f'TransformType={ttype},PerAxis=(bX={str(per_axis.get("bX",False)).lower()},'
            f'bY={str(per_axis.get("bY",False)).lower()},bZ={str(per_axis.get("bZ",False)).lower()}))'
        )
    weights = [format_float(w) for w in node.get("ConstraintWeights", [])]
    posX, posY = _node_pos(index)

    out = f'Begin Object Class=/Script/AnimGraph.AnimGraphNode_Constraint Name="{key}"\n'
    out += f'   Node=(BoneToModify=(BoneName="{bone}"),ConstraintSetup=({",".join(setup_entries)}),ConstraintWeights=({",".join(weights)}))\n'
    out += '   ShowPinForProperties(0)=(PropertyName="ComponentPose",bShowPin=True)\n'
    out += '   ShowPinForProperties(1)=(PropertyName="bAlphaBoolEnabled",bShowPin=True)\n'
    out += '   ShowPinForProperties(2)=(PropertyName="Alpha",bShowPin=True)\n'
    out += '   ShowPinForProperties(3)=(PropertyName="AlphaCurveName",bShowPin=True)\n'
    if pin_in_guid and pin_out_guid:
        out += _pin_lines(pin_in_guid, pin_out_guid, prev_key, prev_out_guid, next_key, next_in_guid)
    out += f'   NodePosX={posX}\n'
    out += f'   NodePosY={posY}\n'
    out += 'End Object\n'
    return out

def format_layered_bone_blend_node(key, node, index,
                                    pin_in_guid="", pin_out_guid="",
                                    prev_key="", prev_out_guid="",
                                    next_key="", next_in_guid=""):
    layers = []
    for layer in node.get("LayerSetup", []):
        if layer:
            filters = []
            for f in layer.get("BranchFilters", []):
                if f:
                    filters.append(f'(BoneName="{f.get("BoneName","")}",BlendDepth={int(f.get("BlendDepth",0))})')
            layers.append(f"(BranchFilters=({','.join(filters)}))")

    layer_setup = f"LayerSetup=({','.join(layers)})"
    mesh_space_rot = str(node.get("bMeshSpaceRotationBlend", False))
    mesh_space_scale = str(node.get("bMeshSpaceScaleBlend", False))
    curve_blend = node.get("CurveBlendOption", "").split("::")[-1]
    blend_root = str(node.get("bBlendRootMotionBasedOnRootBone", False))

    weights_part = ""
    blend_weights = node.get("BlendWeights", [])
    if blend_weights:
        weights_part = f",BlendWeights=({','.join(format_float(w) for w in blend_weights)})"

    posX, posY = _node_pos(index)

    out = f'Begin Object Class=/Script/AnimGraph.AnimGraphNode_LayeredBoneBlend Name="{key}"\n'
    out += (f'   Node=({layer_setup},bMeshSpaceRotationBlend={mesh_space_rot},'
            f'bMeshSpaceScaleBlend={mesh_space_scale},CurveBlendOption={curve_blend},'
            f'bBlendRootMotionBasedOnRootBone={blend_root}{weights_part})\n')
    if pin_in_guid and pin_out_guid:
        out += _pin_lines(pin_in_guid, pin_out_guid, prev_key, prev_out_guid, next_key, next_in_guid)
    out += f'   NodePosX={posX}\n'
    out += f'   NodePosY={posY}\n'
    out += 'End Object\n'
    return out

def format_spring_bone_node(key, node, index,
                             pin_in_guid="", pin_out_guid="",
                             prev_key="", prev_out_guid="",
                             next_key="", next_in_guid=""):
    bone = node.get("SpringBone", {}).get("BoneName", "")
    max_disp = format_float(node.get("MaxDisplacement", 0))
    stiffness = format_float(node.get("SpringStiffness", 0))
    damping = format_float(node.get("SpringDamping", 0))
    error_thresh = format_float(node.get("ErrorResetThresh", 0))
    b = lambda val: str(node.get(val, False))
    posX, posY = _node_pos(index)

    out = f'Begin Object Class=/Script/AnimGraph.AnimGraphNode_SpringBone Name="{key}"\n'
    out += (f'   Node=(SpringBone=(BoneName="{bone}"),MaxDisplacement={max_disp},'
            f'SpringStiffness={stiffness},SpringDamping={damping},ErrorResetThresh={error_thresh},'
            f'bLimitDisplacement={b("bLimitDisplacement")},bTranslateX={b("bTranslateX")},'
            f'bTranslateY={b("bTranslateY")},bTranslateZ={b("bTranslateZ")},'
            f'bRotateX={b("bRotateX")},bRotateY={b("bRotateY")},bRotateZ={b("bRotateZ")})\n')
    out += '   ShowPinForProperties(0)=(PropertyName="ComponentPose",bShowPin=True)\n'
    out += '   ShowPinForProperties(1)=(PropertyName="bAlphaBoolEnabled",bShowPin=True)\n'
    out += '   ShowPinForProperties(2)=(PropertyName="Alpha",bShowPin=True)\n'
    out += '   ShowPinForProperties(3)=(PropertyName="AlphaCurveName",bShowPin=True)\n'
    if pin_in_guid and pin_out_guid:
        out += _pin_lines(pin_in_guid, pin_out_guid, prev_key, prev_out_guid, next_key, next_in_guid)
    out += f'   NodePosX={posX}\n'
    out += f'   NodePosY={posY}\n'
    out += 'End Object\n'
    return out

NODE_FORMATTERS = {
    "AnimGraphNode_KawaiiPhysics":    format_kawaii_physics_node,
    "AnimGraphNode_ModifyBone":       format_modify_bone_node,
    "AnimGraphNode_Constraint":       format_constraint_node,
    "AnimGraphNode_LayeredBoneBlend": format_layered_bone_blend_node,
    "AnimGraphNode_SpringBone":       format_spring_bone_node,
}

KNOWN_PREFIXES = tuple(NODE_FORMATTERS.keys())

def detect_anim_classes(json_path: str) -> list:
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    candidates = []
    for item in data:
        type_name = item.get("Type", "")
        props = item.get("Properties", {})
        if any(k.startswith(KNOWN_PREFIXES) for k in props):
            candidates.append(type_name)
    return candidates

def convert(json_path: str, anim_bp_class: str, connect_nodes: bool = False) -> str:
    with open(json_path, "r", encoding="utf-8") as f:
        json_content = json.load(f)

    target = next((item for item in json_content if item.get("Type") == anim_bp_class), None)
    if not target:
        raise ValueError(f"Could not find '{anim_bp_class}' object in JSON.")

    props = target.get("Properties", {})

    nodes_to_format = []
    for key, node in props.items():
        formatter = next(
            (fn for prefix, fn in NODE_FORMATTERS.items() if key.startswith(prefix)),
            None,
        )
        if formatter:
            nodes_to_format.append((key, node, formatter))

    n = len(nodes_to_format)

    in_guids = [_new_guid() for _ in range(n)] if connect_nodes else [""] * n
    out_guids = [_new_guid() for _ in range(n)] if connect_nodes else [""] * n

    output_parts = []
    for i, (key, node, formatter) in enumerate(nodes_to_format):
        prev_key = nodes_to_format[i - 1][0] if connect_nodes and i > 0     else ""
        prev_out_guid = out_guids[i - 1]           if connect_nodes and i > 0     else ""
        next_key = nodes_to_format[i + 1][0]  if connect_nodes and i < n - 1 else ""
        next_in_guid = in_guids[i + 1]            if connect_nodes and i < n - 1 else ""

        output_parts.append(
            formatter(key, node, i,
                      pin_in_guid=in_guids[i],
                      pin_out_guid=out_guids[i],
                      prev_key=prev_key,
                      prev_out_guid=prev_out_guid,
                      next_key=next_key,
                      next_in_guid=next_in_guid)
        )

    return "\n".join(output_parts)

class ConvertWorker(QThread):
    finished = pyqtSignal(str)
    error = pyqtSignal(str)

    def __init__(self, json_path, anim_bp_class, connect_nodes=False):
        super().__init__()
        self.json_path = json_path
        self.anim_bp_class = anim_bp_class
        self.connect_nodes = connect_nodes

    def run(self):
        try:
            self.finished.emit(convert(self.json_path, self.anim_bp_class, self.connect_nodes))
        except Exception as exc:
            self.error.emit(str(exc))

class PickClassDialog(QMessageBox):
    def __init__(self, candidates: list, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Multiple Classes Found")
        self.setText("Multiple AnimBP classes were detected.\nChoose one to use:")
        self.setIcon(QMessageBox.Icon.Question)

        self._combo = QComboBox()
        self._combo.addItems(candidates)
        self.layout().addWidget(self._combo, 1, 1)

        self.addButton(QMessageBox.StandardButton.Ok)
        self.addButton(QMessageBox.StandardButton.Cancel)

    def chosen(self) -> str:
        return self._combo.currentText()

class MainWindow(QMainWindow):
    def __init__(self, theme_name: str = "dark"):
        super().__init__()
        self.setWindowIcon(QIcon(resource_path("icons8-sync-64.ico")))
        self.setWindowTitle("Json2AnimBP")
        self.setMinimumSize(520, 600)
        self.setAcceptDrops(True)
        self._last_result = ""
        self._json_path = ""
        self._theme_name = theme_name
        self._build_ui()

    def _c(self, key: str) -> str:
        return THEMES[self._theme_name][key]

    def _toggle_theme(self):
        self._theme_name = "light" if self._theme_name == "dark" else "dark"
        save_theme_preference(self._theme_name)
        QApplication.instance().setPalette(build_palette(self._theme_name))

        self.theme_btn.setText(self._c("theme_btn_label"))

        if self._json_path:
            self.file_label.setStyleSheet(f"color: {self._c('ok')};")
        else:
            self.file_label.setStyleSheet(f"color: {self._c('dim')};")

        self.status_bar.setStyleSheet("")

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            urls = event.mimeData().urls()
            if len(urls) == 1 and urls[0].toLocalFile().lower().endswith(".json"):
                event.acceptProposedAction()
                return
        event.ignore()

    def dropEvent(self, event):
        path = event.mimeData().urls()[0].toLocalFile()
        self._load_json_file(path)

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setSpacing(10)
        root.setContentsMargins(12, 12, 12, 12)

        file_group = QGroupBox("JSON File")
        file_layout = QHBoxLayout(file_group)
        self.file_label = QLabel("No file loaded, drag & drop anywhere in this window or browse.")
        self.file_label.setStyleSheet(f"color: {self._c('dim')};")
        file_layout.addWidget(self.file_label, stretch=1)
        browse_btn = QPushButton("Browse…")
        browse_btn.setFixedWidth(90)
        browse_btn.clicked.connect(self._browse_json)
        file_layout.addWidget(browse_btn)
        root.addWidget(file_group)

        class_group = QGroupBox("AnimBP Class")
        class_layout = QHBoxLayout(class_group)

        self.class_edit = QLineEdit()
        self.class_edit.setPlaceholderText(
            "Type the AnimBP class name here, or use Auto-Detect →"
        )
        class_layout.addWidget(self.class_edit)

        self.detect_btn = QPushButton("Auto-Detect")
        self.detect_btn.setFixedWidth(130)
        self.detect_btn.setToolTip(
            "Scan the loaded JSON and find all objects that contain AnimGraph nodes"
        )
        self.detect_btn.clicked.connect(self._auto_detect_class)
        class_layout.addWidget(self.detect_btn)

        root.addWidget(class_group)

        out_group = QGroupBox("Output Options")
        out_layout = QVBoxLayout(out_group)

        self.copy_checkbox = QCheckBox("Copy output to clipboard")
        self.copy_checkbox.setChecked(True)
        if not PYPERCLIP_AVAILABLE:
            self.copy_checkbox.setEnabled(False)
            self.copy_checkbox.setToolTip("pyperclip not installed - run: pip install pyperclip")
        out_layout.addWidget(self.copy_checkbox)

        self.save_checkbox = QCheckBox("Save to .txt file")
        self.save_checkbox.setChecked(False)
        self.save_checkbox.stateChanged.connect(self._toggle_save_path)
        out_layout.addWidget(self.save_checkbox)

        self.save_path_widget = QWidget()
        path_row = QHBoxLayout(self.save_path_widget)
        path_row.setContentsMargins(0, 0, 0, 0)
        path_row.addWidget(QLabel("Output File:"))
        self.output_path_edit = QLineEdit()
        self.output_path_edit.setPlaceholderText("Path for the .txt output file…")
        path_row.addWidget(self.output_path_edit)
        out_browse_btn = QPushButton("Browse…")
        out_browse_btn.setFixedWidth(90)
        out_browse_btn.clicked.connect(self._browse_output)
        path_row.addWidget(out_browse_btn)
        out_layout.addWidget(self.save_path_widget)
        self.save_path_widget.setVisible(False)                                   
        self.save_checkbox.stateChanged.connect(self._toggle_save_path)

        connect_row = QHBoxLayout()
        self.connect_checkbox = QCheckBox("Connect nodes in chain")
        self.connect_checkbox.setChecked(False)
        self.connect_checkbox.setToolTip(
            "Adds CustomProperties Pin entries to wire nodes into a linear chain.\n"
            "Behaviour may vary between platforms - test before relying on it."
        )
        connect_row.addWidget(self.connect_checkbox)
        connect_row.addStretch()
        out_layout.addLayout(connect_row)

        root.addWidget(out_group)

        btn_row = QHBoxLayout()
        self.convert_btn = QPushButton("Convert")
        self.convert_btn.setFixedHeight(38)
        f = self.convert_btn.font()
        f.setPointSize(10)
        f.setBold(True)
        self.convert_btn.setFont(f)
        self.convert_btn.clicked.connect(self._run_conversion)
        btn_row.addWidget(self.convert_btn)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.setFixedHeight(38)
        self.clear_btn.setFixedWidth(80)
        self.clear_btn.clicked.connect(self._clear_all)
        btn_row.addWidget(self.clear_btn)
        root.addLayout(btn_row)

        preview_group = QGroupBox("Preview")
        preview_layout = QVBoxLayout(preview_group)
        self.preview = QTextEdit()
        self.preview.setReadOnly(True)
        self.preview.setFont(QFont("Consolas,Courier New", 9))
        self.preview.setPlaceholderText("Converted output will appear here…")
        preview_layout.addWidget(self.preview)
        root.addWidget(preview_group, stretch=1)

        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        if not PYPERCLIP_AVAILABLE:
            self.status_bar.showMessage(
                "⚠  pyperclip not found - install with: pip install pyperclip"
            )
        else:
            self.status_bar.showMessage("Drop or browse a JSON file to get started.")

        self.theme_btn = QPushButton(self._c("theme_btn_label"))
        self.theme_btn.setFixedWidth(90)
        self.theme_btn.setFlat(True)
        self.theme_btn.setToolTip("Toggle dark / light theme")
        self.theme_btn.clicked.connect(self._toggle_theme)
        self.status_bar.addPermanentWidget(self.theme_btn)

    def _load_json_file(self, path: str):
        self._json_path = path
        filename = os.path.basename(path)
        self.file_label.setText(f"✔  {filename}")
        self.file_label.setStyleSheet(f"color: {self._c('ok')};")

        if not self.output_path_edit.text():
            base = os.path.splitext(path)[0]
            self.output_path_edit.setText(base + "_AnimBP.txt")

        if not self.class_edit.text().strip():
            self._auto_detect_class(silent=True)
        else:
            self._set_status(f"Loaded: {filename}")

    def _browse_json(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Select JSON File", "", "JSON Files (*.json);;All Files (*)"
        )
        if path:
            self._load_json_file(path)

    def _browse_output(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Output As", "", "Text Files (*.txt);;All Files (*)"
        )
        if path:
            self.output_path_edit.setText(path)

    def _auto_detect_class(self, silent: bool = False):
        if not self._json_path or not os.path.isfile(self._json_path):
            if not silent:
                self._set_status("⚠  Load a JSON file first.", error=True)
            return

        try:
            candidates = detect_anim_classes(self._json_path)
        except Exception as e:
            self._set_status(f"⚠  Detection failed: {e} - type the class name manually.", error=True)
            self.class_edit.setFocus()
            return

        if not candidates:
            self._set_status(
                "⚠  No AnimGraph objects found - type the class name manually.", error=True
            )
            self.class_edit.setFocus()
            return

        if len(candidates) == 1:
            self.class_edit.setText(candidates[0])
            self._set_status(f"✔  Auto-detected: {candidates[0]}")
        else:

            dialog = PickClassDialog(candidates, self)
            if dialog.exec():
                chosen = dialog.chosen()
                self.class_edit.setText(chosen)
                self._set_status(f"✔  Selected: {chosen}")

    def _run_conversion(self):
        if not self._json_path or not os.path.isfile(self._json_path):
            self._set_status("⚠  Please load a JSON file first.", error=True)
            return
        anim_class = self.class_edit.text().strip()
        if not anim_class:
            self._set_status("⚠  AnimBP Class cannot be empty.", error=True)
            return

        self.convert_btn.setEnabled(False)
        self._set_status("Converting…")
        self.preview.clear()

        self._worker = ConvertWorker(self._json_path, anim_class,
                                     connect_nodes=self.connect_checkbox.isChecked())
        self._worker.finished.connect(self._on_finished)
        self._worker.error.connect(self._on_error)
        self._worker.start()

    def _on_finished(self, result: str):
        self._last_result = result
        self.convert_btn.setEnabled(True)
        self.preview.setPlainText(result)
        node_count = result.count("Begin Object")
        messages = [f"{node_count} node(s) converted"]

        if self.save_checkbox.isChecked():
            output_path = self.output_path_edit.text().strip()
            if output_path:
                try:
                    with open(output_path, "w", encoding="utf-8") as f:
                        f.write(result)
                    messages.append(f"saved → {os.path.basename(output_path)}")
                except Exception as e:
                    self._set_status(f"⚠  Could not save file: {e}", error=True)
                    return
            else:
                messages.append("no output path set - file not saved")

        if self.copy_checkbox.isChecked() and PYPERCLIP_AVAILABLE:
            try:
                pyperclip.copy(result)
                messages.append("copied to clipboard")
            except Exception as e:
                messages.append(f"clipboard failed ({e})")

        self._set_status("✔  " + " | ".join(messages))

    def _on_error(self, message: str):
        self.convert_btn.setEnabled(True)
        self.preview.setPlainText(f"[ERROR]\n{message}")
        self._set_status(f"⚠  {message}", error=True)

    def _toggle_save_path(self, state):
        self.save_path_widget.setVisible(bool(state))

    def _clear_all(self):
        self._json_path = ""
        self._last_result = ""
        self.file_label.setText("No file loaded.")
        self.file_label.setStyleSheet(f"color: {self._c('dim')};")
        self.class_edit.clear()
        self.output_path_edit.clear()
        self.preview.clear()
        self._set_status("Cleared.")

    def _set_status(self, text: str, error: bool = False):
        self.status_bar.showMessage(text)
        self.status_bar.setStyleSheet(f"color: {self._c('error')};" if error else "")

def main():
    if sys.platform == "win32":
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("Json2AnimBP")

    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setWindowIcon(QIcon(resource_path("icons8-sync-64.ico")))

    theme_name = load_theme_preference()
    app.setPalette(build_palette(theme_name))

    window = MainWindow(theme_name=theme_name)
    window.show()

    argv_path = sys.argv[1] if len(sys.argv) > 1 else ""
    if argv_path and argv_path.lower().endswith(".json") and os.path.isfile(argv_path):
        def _auto_run():
            window._load_json_file(argv_path)
            QTimer.singleShot(200, window._run_conversion)
        QTimer.singleShot(100, _auto_run)

    sys.exit(app.exec())

if __name__ == "__main__":
    main()