hyper_parameters = {
    'observation_window_size': 100,
    'delta_max_len': 5,
    'prefetch_number': 64,
    'delta_frequency_threshold': 0.8,
    'hotspot_budget': 20,
    'affinity_score_threshold': 0.999,
}

cached_pages = dict()
accessed_pages = list()
cache_miss_pages = list()
all_prefetched_pages = dict()

class hotspot:
    def __init__(self, addr, time):
        self.addr_observation_window = [addr]
        self.latest_accessed_time = time
        self.observation_window_size = hyper_parameters['observation_window_size']
        self.delta_max_len = hyper_parameters['delta_max_len']
        self.delta = None
        self.delta_len = 0
        self.prefetch_number = hyper_parameters['prefetch_number']
        self.frequency_threshold = hyper_parameters['delta_frequency_threshold']
        #print('Create hotspot; addr: '+str(addr)+'; time: '+str(time))

    def affinity_to_addr(self, addr):
        min_addr = min(addr, self.addr_observation_window[-1])
        max_addr = max(addr, self.addr_observation_window[-1])
        affinity_score = float(min_addr)/float(max_addr)
        return affinity_score
    
    def update_hotspot(self, addr, time):
        self.latest_accessed_time = time
        self.addr_observation_window.append(addr)
        if len(self.addr_observation_window) > self.observation_window_size:
            self.addr_observation_window.pop(0)
        if len(self.addr_observation_window) < self.observation_window_size/8:
            return -1
        #print(self.addr_observation_window)

        best_delta_len = 0
        best_delta_len_frequency = 0
        best_delta = None

        # seek best delta_len
        for delta_len_tried in range(1, self.delta_max_len + 1):
            tmp_deltas = []
            for i in range(delta_len_tried, len(self.addr_observation_window)):
                tmp_deltas.append(
                    self.addr_observation_window[i] - 
                    self.addr_observation_window[i - delta_len_tried]
                    )
            
            delta_freq_dict = {}
            for i in tmp_deltas:
                if i not in delta_freq_dict:
                    delta_freq_dict[i] = 1
                else:
                    delta_freq_dict[i] += 1
            
            chosen_delta_at_len = max(delta_freq_dict, key=delta_freq_dict.get)
            chosen_delta_freq_at_len = delta_freq_dict[chosen_delta_at_len] / float(len(tmp_deltas))
            # print('Chosen_delta value: ' + str(chosen_delta_at_len) + 
            #     " at_len: " + str(delta_len_tried) + 
            #     "; chosen_delta_freq: " + str(chosen_delta_freq_at_len))
            if best_delta_len_frequency < chosen_delta_freq_at_len:
                best_delta_len_frequency = chosen_delta_freq_at_len
                best_delta_len = delta_len_tried
                best_delta = chosen_delta_at_len
        
        #print('best_delta_len_frequency: ' + str(best_delta_len_frequency) +
        #"; frequency_threshold: " + str(self.frequency_threshold))

        if best_delta_len_frequency > self.frequency_threshold:
            # conduct prefetch if the frequency is above a threshold
            self.delta_len = best_delta_len
            self.delta = best_delta
            #print('Identified delta len: ' + str(self.delta_len) +
            #'; delta value: ' + str(self.delta))
            return self.generate_prefetch_candidates()
        else:
            return -1

    def generate_prefetch_candidates(self):
        pages_to_prefetch = []
        start_point = self.addr_observation_window[-1 * self.delta_len:]
        for i in range(self.prefetch_number):
            page_to_prefetch = start_point[i%self.delta_len] + self.delta*(1+i/self.delta_len)
            if page_to_prefetch not in cached_pages:
                pages_to_prefetch.append(page_to_prefetch)
        #print('Pages to prefetch: '+ str(pages_to_prefetch))
        return pages_to_prefetch
    
class hotspot_table:
    def __init__(self):
        self.hotspot_list = []
        self.hotspot_budget = hyper_parameters['hotspot_budget']
        self.affinity_score_threshold = hyper_parameters['affinity_score_threshold']

    def update_hotspot_table(self, addr, time):
        # find nearest hotspot based on affinity score with each other
        tmp_score = [i.affinity_to_addr(addr) for i in self.hotspot_list]
        #print('affinity score: ' +str(tmp_score))
        if len(self.hotspot_list) > 0 and max(tmp_score) > self.affinity_score_threshold:
            activated_hotspot = self.hotspot_list[tmp_score.index(max(tmp_score))]
            prefetch_candidates = activated_hotspot.update_hotspot(addr, time)
            return prefetch_candidates
        else:
            # create new hotspot if necessary
            new_hotspot = hotspot(addr, time)
            self.hotspot_list.append(new_hotspot)
            if len(self.hotspot_list) > self.hotspot_budget: # FIFO
                hotspot_to_remove = min(self.hotspot_list, key=lambda x:x.latest_accessed_time)
                self.hotspot_list.remove(hotspot_to_remove)
                #print("Removed: " + str(hotspot_to_remove))
            return -1

def do_main(fname):
    f = open(fname, 'r')
    d = f.readlines()
    f.close()
    
    hotspots = hotspot_table()
    for i in range(len(d)):
        ds = d[i].split(' ')
        time = int(ds[0])
        addr = int(ds[2],16)
        accessed_pages.append(addr)
        if addr not in cached_pages: # cache miss occurs
            cached_pages[addr] = (True, time) # fetch request page
            cache_miss_pages.append(addr)
            pages_to_prefetch = hotspots.update_hotspot_table(addr, time)
            if pages_to_prefetch != -1:
                for p in pages_to_prefetch:
                    all_prefetched_pages[p] = (False, -1)
                    cached_pages[p] = (False, -1)
        else: # cache hit occurs
            cached_pages[addr] = (True, time)
            if addr in all_prefetched_pages:
                all_prefetched_pages[addr] = (True, time)
    
    # analyse performance
    # print(hyper_parameters)
    accessed_prefetch_pages_number = len([i for i in all_prefetched_pages if all_prefetched_pages[i][0]])
    accuracy = float(accessed_prefetch_pages_number) / len(all_prefetched_pages)
    coverage = float(accessed_prefetch_pages_number) / (accessed_prefetch_pages_number + len(cache_miss_pages))
    result = "Prefetch Accuracy:{}, accessed_prefetch_pages:{}, total prefetched pages:{}, coverage:{}".format(accuracy, 
        accessed_prefetch_pages_number, 
        len(all_prefetched_pages), 
        coverage)
    print(result)

if __name__=='__main__':
    fname = ['test00.txt','test01.txt','test02.txt','test03.txt']
    for fn in fname:
        fn = './data/'+fn
        do_main(fn)
