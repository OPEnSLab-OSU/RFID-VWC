"{\
	'general':\
	{\
		'name':'Device',\
		'instance':1,\
		'interval':1000,\
    'print_verbosity':2\
	},\
	'components':[\
		{\
			'name':'Multiplexer',\
			'params':'default'\
		},\
    {\
      'name':'SD',\
      'params':[true,1000, 13, 'test', true]\ 
    },\
    {\
      'name':'DS3231',\
      'params':[11,true]\
    },\
    {\
      'name':'Interrupt_Manager',\
      'params':[0]\
    },\
    {\
      'name':'Sleep_Manager',\
      'params':[true,false,1]\
    }\
	]\
}"
