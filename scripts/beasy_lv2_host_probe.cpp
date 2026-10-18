#include <dlfcn.h>
#include <lv2/core/lv2.h>
#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct Saved { uint32_t key=0,type=0,flags=0; std::vector<uint8_t> data; };
struct UriMap { std::map<std::string, LV2_URID> ids; std::vector<std::string> uris; };
static LV2_URID map_uri(LV2_URID_Map_Handle h, const char* uri) {
  auto* m = static_cast<UriMap*>(h); auto it=m->ids.find(uri); if(it!=m->ids.end()) return it->second;
  LV2_URID id = static_cast<LV2_URID>(m->uris.size()+1); m->ids.emplace(uri,id); m->uris.emplace_back(uri); return id;
}
static void die(const char* s) { std::fprintf(stderr,"FAIL %s\n",s); std::exit(2); }
static LV2_State_Status store_state(LV2_State_Handle h,uint32_t key,const void* value,size_t size,uint32_t type,uint32_t flags) { auto* v=static_cast<std::vector<Saved>*>(h); Saved x; x.key=key;x.type=type;x.flags=flags;x.data.assign(static_cast<const uint8_t*>(value),static_cast<const uint8_t*>(value)+size); v->push_back(std::move(x)); return LV2_STATE_SUCCESS; }
static const void* retrieve_state(LV2_State_Handle h,uint32_t key,size_t* size,uint32_t* type,uint32_t* flags) { auto* v=static_cast<std::vector<Saved>*>(h); for(auto& x:*v) if(x.key==key){if(size)*size=x.data.size();if(type)*type=x.type;if(flags)*flags=x.flags;return x.data.data();} return nullptr; }

int main(int argc,char**argv) {
  if(argc != 2) { std::fprintf(stderr,"usage: host Beasy.so\n"); return 2; }
  void* lib=dlopen(argv[1], RTLD_NOW|RTLD_LOCAL); if(!lib) die(dlerror());
  using Fn=const LV2_Descriptor*(*)(uint32_t); auto get=(Fn)dlsym(lib,"lv2_descriptor"); if(!get) die("lv2_descriptor missing");
  const LV2_Descriptor* d=get(0); if(!d || std::strcmp(d->URI,"http://pulp.audio/plugins/beasy")!=0) die("descriptor mismatch");
  if(get(1)!=nullptr) die("unexpected descriptor count");
  UriMap map; LV2_URID_Map map_feature{&map,map_uri}; LV2_Feature mapf{LV2_URID__map,&map_feature}; const LV2_Feature* features[]={&mapf,nullptr};
  std::string bundle=argv[1]; auto slash=bundle.find_last_of('/'); bundle.resize(slash+1);
  LV2_Handle h=d->instantiate(d,48000.0,bundle.c_str(),features); if(!h) die("instantiate failed");
  constexpr uint32_t frames=256; std::vector<std::vector<float>> audio(4,std::vector<float>(frames,0.0f));
  std::vector<float> ctl(15,0.0f); // ports 4..18 (latency output is index 18)
  ctl[0]=0.5f; ctl[1]=0.5f; ctl[2]=0.5f; ctl[3]=0.0f; ctl[4]=0.5f; ctl[5]=0.0f; ctl[6]=0.5f; ctl[7]=0.0f; ctl[8]=0.0f; ctl[9]=0.0f; ctl[9]=3.0f; ctl[10]=0.0f; ctl[11]=0.0f; ctl[12]=1.0f; ctl[13]=0.0f; ctl[14]=0.0f;
  for(uint32_t i=0;i<4;i++) d->connect_port(h,i,audio[i].data());
  for(uint32_t i=0;i<15;i++) d->connect_port(h,4+i,&ctl[i]);
  std::vector<uint8_t> atom(32768,0); auto* seq=reinterpret_cast<LV2_Atom_Sequence*>(atom.data()); seq->atom.size=sizeof(LV2_Atom_Sequence_Body); seq->atom.type=map_uri(&map,LV2_ATOM__Sequence); seq->body.unit=0; seq->body.pad=0;
  auto* ev=reinterpret_cast<LV2_Atom_Event*>(atom.data()+sizeof(LV2_Atom_Sequence)); ev->time.frames=0; ev->body.type=map_uri(&map,LV2_MIDI__MidiEvent); ev->body.size=3; auto* msg=reinterpret_cast<uint8_t*>(LV2_ATOM_BODY(&ev->body)); msg[0]=0x90; msg[1]=60; msg[2]=110; seq->atom.size=sizeof(LV2_Atom_Sequence_Body)+((sizeof(LV2_Atom_Event)+3+7)&~7);
  d->connect_port(h,17,atom.data()); d->activate ? d->activate(h) : (void)0;
  double peak=0; size_t finite=0; for(int block=0; block<400; ++block) { std::fill(audio[2].begin(),audio[2].end(),0.0f); d->run(h,frames); for(float x:audio[2]) { if(std::isfinite(x)) finite++; peak=std::max(peak,std::abs((double)x)); } }
  std::printf("descriptor=%s\nports=19\nurid_ids=%zu\nfinite_output_frames=%zu/%u\npeak=%0.9f\nlatency=%0.3f\n",d->URI,map.uris.size(),finite,frames,peak,ctl[14]);
  const auto* ext=d->extension_data ? static_cast<const LV2_State_Interface*>(d->extension_data(LV2_STATE__interface)) : nullptr;
  std::vector<Saved> saved; LV2_State_Status save_status=LV2_STATE_ERR_UNKNOWN, restore_status=LV2_STATE_ERR_UNKNOWN;
  if(ext) { save_status=ext->save(h,store_state,&saved,LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE,features); restore_status=ext->restore(h,retrieve_state,&saved,0,features); }
  std::printf("state_extension=%s\n",ext?"present":"absent"); std::printf("state_save_status=%d\nstate_restore_status=%d\nstate_values=%zu\n",(int)save_status,(int)restore_status,saved.size());
  if(d->deactivate) d->deactivate(h); d->cleanup(h); dlclose(lib); if(finite != 400*frames || !ext || save_status != LV2_STATE_SUCCESS || restore_status != LV2_STATE_SUCCESS) return 3; return 0;
}
