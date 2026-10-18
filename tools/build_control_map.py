import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
draft = json.loads((root / 'provenance/control-map.draft.json').read_text())
W, H = draft['source']['panel_mm']['width'], draft['source']['panel_mm']['height']
view_w, view_h = 200.693, 364.252
named = {c['id']: c for c in draft['controls']}
handle_to_name = {
    '2472':'density', '2473':'time', '2474':'pitch', '2470':'size', '2471':'shape',
    # The seven auxiliary pots are source-identified in the PCB/schematic
    # audit and now have explicit Beasy parameter contracts. Keep this table
    # adjacent to the geometry import so a new control cannot become bound
    # merely because an editor placement happens to exist.
    '2477':'feedback', '247A':'dry_wet', '247B':'reverb',
    '2475':'time_cv_amount', '2476':'size_cv_amount',
    '2478':'shape_cv_amount', '2479':'pitch_cv_amount',
}
controls=[]
for g in draft['dxf_knob_geometry']:
    handle=g['handle']; parameter=handle_to_name.get(handle)
    if parameter:
        cid=parameter; binding='bound'; role=named.get(parameter, {'role': 'auxiliary_parameter'})['role']; automation=f'beasy.{parameter}'
    elif g['class']=='small':
        cid=f"small-{sum(1 for x in controls if x['id'].startswith('small-'))+1:02d}"; binding='provisional'; role='auxiliary_parameter_candidate'; automation=None
    else:
        cid=f"aux-{sum(1 for x in controls if x['id'].startswith('aux-'))+1:02d}"; binding='provisional'; role='auxiliary_control_candidate'; automation=None
    x,y=g['center_mm']['x'],g['center_mm']['y']; radius=g['radius_mm']
    scale=1.3 if parameter else 1.5
    controls.append({
      'id':cid, 'role':role, 'binding_status':binding, 'parameter_id':parameter,
      'automation_id':automation, 'geometry_ref':{'source':'dxf','handle':handle,'layer':g['layer'],'class':g['class'],'center_mm':g['center_mm'],'radius_mm':radius},
      'normalized':{'center':{'x':round(x/W,9),'y':round((H-y)/H,9)},'radius':round(radius/W,9)},
      'viewbox_pt':{'center':{'x':round(x*view_w/W,6),'y':round((H-y)*view_h/H,6)},'radius':round(radius*view_w/W,6)},
      'hit_target':{'shape':'circle','radius_mm':round(radius*scale,3),'interaction':'drag_vertical_or_horizontal','minimum_touch_mm':12.0 if parameter else 8.0},
      'skin_ref': {'major-ornate':'major-ornate','major-large':'major-large','small':'small-black','auxiliary':'auxiliary'}[g['class']],
      'evidence': ('dxf-placement-plus-parameter-contract-and-source-audit'
                   if binding == 'bound' and handle not in {'2472','2473','2474','2470','2471'}
                   else ('dxf-placement-plus-parameter-contract' if binding == 'bound'
                         else 'dxf-placement-only'))
    })
control_map={
 '$schema':'https://json-schema.org/draft/2020-12/schema',
 'schema':'beasy.control-map.v1', 'status':'geometry-validated-parameter-bindings-partial',
 'product':{'name':'Beasy','affiliation':'independent','artwork_use':'provisional-calibration-only'},
 'source':{
   'dxf':draft['source']['dxf'],'dxf_sha256':draft['source']['dxf_sha256'],'units':'millimetres','panel_mm':draft['source']['panel_mm'],
   'coordinate_system':draft['source']['coordinate_system'],'ai_page_pt':{'width':view_w,'height':view_h},
   'artwork_rights':{'state':'pending_rights_review','redistribution':'blocked_until_cleared','basis':'provenance/source-manifest.json'}
 },
 'normalization':{'ui_origin':'top-left','source_origin':'bottom-left','y_axis':'flipped','formulas':draft['normalization'],'viewbox_pt':{'width':view_w,'height':view_h}},
 'controls':controls,
 'validation':{
   'validator':'tools/validate_control_map.py','geometry_tolerance_mm':0.01,'expected_dxf_knob_count':24,
   'screenshot_overlay':{'status':'not-run','reason':'requires resolved artwork rights and straight-on calibration'},
   'parameter_binding_review':{'status':'partial',
      'bound_ids':['density','time','pitch','size','shape','feedback','dry_wet','reverb',
                   'time_cv_amount','size_cv_amount','shape_cv_amount','pitch_cv_amount'],
      'provisional_count':12,
      'source_audit':'provenance/control-map-source-audit-20261018.json'}
 },
 'rights':{'panel_artwork':'pending_rights_review','knob_material':'provisional-calibration-only','trademarks':'non_affiliation_review_required'}
}
(root/'provenance/control-map.json').write_text(json.dumps(control_map,indent=2)+'\n')
