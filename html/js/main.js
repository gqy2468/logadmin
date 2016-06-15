$(document).ready(function(){
	$.ajax({   
		url:'/?action=logconf',   
		type:'get',
		data:'',
		error:function(){   
			alert('Get Log Conf Error!');   
		},   
		success:function(data){
			var arr = data.split("\n");
			var html = '<li class="active"><a href="/"><span class="glyphicon glyphicon-dashboard"></span> Home</a></li>';
			$.each(arr, function(key, val){
				if (val) {
					var vals = val.split(";");
					var count = vals.length;
					html += '<li class="parent "><a href="javascript:;"><span class="glyphicon glyphicon-list"></span> ' + vals[0];
					html += ' <span data-toggle="collapse" href="#sub-item-' + key + '" class="icon pull-right"><em class="glyphicon glyphicon-s glyphicon-plus"></em></span></a>';
					html += '<ul class="children collapse' + (key ? '' : ' in') + '" id="sub-item-' + key + '">';
					for (var i = 1; i < count; i++) {
						if (vals[i]) {
							html += '<li><a href="javascript:log_list(\'' + vals[i] + '\');"><span class="glyphicon glyphicon-share-alt"></span> ' + vals[i] + '</a></li>';
						}
					}
					html += '</ul></li>';
				}
			});
			html += '<li role="presentation" class="divider"></li>';
			html += '<li><a href="login.html"><span class="glyphicon glyphicon-user"></span> Login Page</a></li>';
			$("#sidebar-collapse .menu").html(html);   
		}
	});

	$.ajax({   
		url:'/?action=loginfo&logname=/var/log/yum.log&logreg=.*&pagenum=1',   
		type:'get',
		data:'',
		error:function(){   
			alert('Get Log Info Error!');   
		},   
		success:function(data){
			var arr = data.split("\n");
			var html = "";
			$.each(arr, function(key, val){
				if (key) {
					html += val + "</br>";
				}
			});
			$(".panel .panel-body").html(html);   
		}
	});
});

$("#pagenum").change(function(){
	log_info();
});

function log_info(logname){
	if (logname) {
		$("#logname").val(logname);
	} else {
		logname = $("#logname").val();
	}
	var logreg = $("#logreg").val();
	var pagenum = $("#pagenum").val();
	pagenum = !pagenum ? 1 : pagenum;
	$.ajax({   
		url:'/?action=loginfo&logname='+logname+'&logreg='+logreg+'&pagenum='+pagenum,   
		type:'get',
		data:'',
		error:function(){   
			alert('Get Log Info Error!');   
		},   
		success:function(data){
			show_nav(logname);
			$(".panel .panel-heading").show(); 
			var arr = data.split("\n");
			var html = "";
			$.each(arr, function(key, val){
				if (key) {
					html += val + "</br>";
				} else {
					var page = "";
					var total = Math.ceil(parseInt(val)/20);
					total = total <= 0 ? 1 : total;
					for (var i = 1; i <= total; i++) {
						page += '<option value="'+i+'"'+(i == pagenum ? ' selected' : '')+'>第'+i+'页</option>';
					}
					$("#pagenum").html(page);
				}
			});
			$(".panel .panel-body").html(html);   
		}
	});
}

function log_list(dirname){
	if (!dirname){
		alert('Get Log List Error!');  
	}
	$.ajax({   
		url:'/?action=loglist&dirname='+dirname,   
		type:'get',
		data:'',
		error:function(){   
			alert('Get Log List Error!');   
		},   
		success:function(data){
			show_nav(dirname);
			$(".panel .panel-heading").hide(); 
			var arr = data.split("\n");
			var html = "";
			$.each(arr, function(key, val){
				if (key) {
					if (val.indexOf('/') > -1) {
						html += "<a href=\"javascript:log_list('" + dirname + val + "');\"><b>" + val + "</b></a></br>";
					} else {
						html += "<a href=\"javascript:log_info('" + dirname + val + "');\">" + val + "</a></br>";
					}
				}
			});
			$(".panel .panel-body").html(html); 
		}
	});
}

function show_nav(logname){
	if (!logname) {
		alert('Show Log Nav Error!');  
	}
	var arr = logname.split("/");
	var total = arr.length;
	if (arr[total - 1]) {
		$(".page-header").html(arr[total - 1]);
		arr.splice(total - 1, 1);
		var dirname = "/";
		var breadcrumb = '<li><a href="/"><span class="glyphicon glyphicon-home"></span></a></li>';
		$.each(arr, function(key, val) {
			if (key) {
				dirname += val + "/";
				breadcrumb += '<li class="active"><a href="javascript:log_list(\'' + dirname + '\');">' + val + '</a></li>';
			}
		});
		$(".breadcrumb").html(breadcrumb);
	} else {
		$(".page-header").html(arr[total - 2]);
		arr.splice(total - 2, 2);
		var dirname = "/";
		var breadcrumb = '<li><a href="/"><span class="glyphicon glyphicon-home"></span></a></li>';
		$.each(arr, function(key, val) {
			if (key) {
				dirname += val + "/";
				breadcrumb += '<li class="active"><a href="javascript:log_list(\'' + dirname + '\');">' + val + '</a></li>';
			}
		});
		$(".breadcrumb").html(breadcrumb);
	}
}