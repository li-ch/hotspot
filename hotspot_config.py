
class hotspot_config:
    def __init__(self, 
        observation_window_size=100,
        delta_max_len=5,
        prefetch_number=64,
        delta_frequency_threshold=0.8,
        hotspot_budget=20,
        affinity_score_threshold=0.999
        ):
        self.observation_window_size = observation_window_size,
        self.delta_max_len = delta_max_len,
        self.prefetch_number = prefetch_number,
        self.delta_frequency_threshold = delta_frequency_threshold,
        self.hotspot_budget = hotspot_budget,
        self.affinity_score_threshold = affinity_score_threshold)
    
    def __str__(self):
        return 'observation_window_size={},delta_max_len={},prefetch_number={},delta_frequency_threshold={},hotspot_budget={},affinity_score_threshold={}'.format(
            self.observation_window_size,
            self.delta_max_len,
            self.prefetch_number,
            self.delta_frequency_threshold,
            self.hotspot_budget,
            self.affinity_score_threshold)